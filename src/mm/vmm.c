#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"

extern uint32_t __kernel_vmem_start;
// #define KERNEL_VMEM_START __kernel_vmem_start

// Simplistic bitmap: one byte per page (0 = free, 1 = used)
static uint8_t vmm_bitmap[VMM_MAX_PAGES];

static void vmm_bitmap_init() {
    memset(vmm_bitmap, 0, sizeof(vmm_bitmap));
}

static int find_free_page() {
    for (int i = 0; i < VMM_MAX_PAGES; i++) {
        if (vmm_bitmap[i] == 0)
            return i;
    }
    return -1;
}

static void set_page_used(int index) {
    vmm_bitmap[index] = 1;
}

static void set_page_free(int index) {
    vmm_bitmap[index] = 0;
}

void vmm_init() {
    // serial_printf("VMM: Initializing virtual memory manager\n");
    
    // Initialize paging structures
    paging_init(0);
    vmm_bitmap_init();

    // Get page directory early to check if initialization succeeded
    uint32_t *pd = get_page_directory();
    if (!pd) {
        serial_printf("VMM: Failed to get page directory!\n");
        return;
    }

    // Before enabling paging, ensure kernel is identity mapped
    // and also mapped to higher half
    extern uint32_t __kernel_physical_start;
    extern uint32_t __kernel_physical_end;
    uint32_t kernel_size = (uint32_t)&__kernel_physical_end - (uint32_t)&__kernel_physical_start;
    uint32_t num_pages = (kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Map kernel pages
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t phys_addr = (uint32_t)&__kernel_physical_start + (i * PAGE_SIZE);
        uint32_t virt_addr = KERNEL_VMEM_START + (i * PAGE_SIZE);
        paging_map_page(phys_addr, phys_addr, PAGE_PRESENT | PAGE_WRITABLE);  // Identity mapping
        paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE);  // Higher half mapping
    }

    // serial_printf("VMM: Page directory at 0x%x\n", (uint32_t)pd);
    paging_enable((uint32_t)pd);

    // Reload segment registers with kernel segments
    __asm__ volatile (
        "mov $0x10, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "mov %ax, %ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
    );
}

uint32_t virt_to_phys(uint32_t virt_addr) {
    if (virt_addr < KERNEL_VMEM_START) {
        return virt_addr; // Identity mapped
    }
    return virt_addr - KERNEL_VMEM_START;
}

uint32_t phys_to_virt(uint32_t phys_addr) {
    return phys_addr + KERNEL_VMEM_START;
}

void *vmm_alloc_page() {
    int page_index = find_free_page();
    if (page_index < 0) {
        serial_printf("VMM: No free virtual pages available\n");
        return 0;
    }
    set_page_used(page_index);
    uint32_t virt_addr = KERNEL_VMEM_START + page_index * PAGE_SIZE;
    
    // Allocate a physical page for mapping
    uint32_t phys_addr = (uint32_t)pmm_alloc_block();
    if (!phys_addr) {
        serial_printf("VMM: Failed to allocate physical page\n");
        set_page_free(page_index);
        return 0;
    }
    
    // Map the physical page to the virtual address
    paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE);
    // serial_printf("VMM: Allocated page - virt: 0x%x, phys: 0x%x\n", virt_addr, phys_addr);
    return (void *)virt_addr;
}

void vmm_free_page(void *addr) {
    if (!addr) return;
    uint32_t virt_addr = (uint32_t)addr;
    if (virt_addr < KERNEL_VMEM_START) return;
    int page_index = (virt_addr - KERNEL_VMEM_START) / PAGE_SIZE;
    // Mark the page as free. Note: unmapping page table entries and freeing physical
    // memory is not fully implemented in this simple example.
    set_page_free(page_index);
}

void* vmm_map_mmio(uintptr_t phys_addr, size_t size, uint32_t flags) {
    // Ensure size is page-aligned
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    int pages_needed = size / PAGE_SIZE;

    // Find contiguous free virtual pages
    int start_index = -1;
    for (int i = 0; i < VMM_MAX_PAGES; i++) {
        if (vmm_bitmap[i] == 0) {
            int j;
            for (j = 0; j < pages_needed; j++) {
                if (i + j >= VMM_MAX_PAGES || vmm_bitmap[i + j] != 0) break;
            }
            if (j == pages_needed) {
                start_index = i;
                break;
            }
        }
    }

    if (start_index < 0) {
        serial_printf("VMM: Not enough contiguous virtual space for MMIO\n");
        return NULL;
    }

    // Mark pages as used
    for (int i = start_index; i < start_index + pages_needed; i++) {
        set_page_used(i);
    }

    // Map physical to virtual
    uint32_t virt_start = KERNEL_VMEM_START + start_index * PAGE_SIZE;
    for (size_t i = 0; i < pages_needed; i++) {
        uintptr_t curr_phys = phys_addr + (i * PAGE_SIZE);
        uintptr_t curr_virt = virt_start + (i * PAGE_SIZE);
        paging_map_page(curr_phys, curr_virt, flags | PAGE_PRESENT | PAGE_WRITABLE);
    }

    return (void*)virt_start;
}