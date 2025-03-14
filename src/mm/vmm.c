#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include <stdbool.h>

extern uint32_t __kernel_vmem_start;
// #define KERNEL_VMEM_START __kernel_vmem_start

uint8_t *vmm_bitmap = NULL;
uint32_t vmm_max_pages = 0;


static bool is_page_free(uint32_t page) {
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    return !(vmm_bitmap[byte_idx] & (1 << bit_idx));
}
static void vmm_bitmap_init() {
    uint32_t total_phys_mem = pmm_get_total_memory();
    vmm_max_pages = total_phys_mem / PAGE_SIZE;
    uint32_t bitmap_size = (vmm_max_pages + 7) / 8; // Bytes needed
    uint32_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Allocate physical memory for the bitmap
    void *bitmap_phys = pmm_alloc_blocks(bitmap_pages);
    if (!bitmap_phys) {
        serial_printf("VMM: Failed to allocate bitmap\n");
        return;
    }

    // Map the bitmap to virtual memory using MMIO
    vmm_bitmap = vmm_map_mmio((uintptr_t)bitmap_phys, bitmap_pages * PAGE_SIZE, PAGE_PRESENT | PAGE_WRITABLE);
    if (!vmm_bitmap) {
        serial_printf("VMM: Failed to map bitmap\n");
        pmm_free_blocks(bitmap_phys, bitmap_pages);
        return;
    }

    memset(vmm_bitmap, 0, bitmap_size);
}

static int find_free_page() {
    for (uint32_t i = 0; i < vmm_max_pages; i++) {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(vmm_bitmap[byte_idx] & (1 << bit_idx))) {
            return i;
        }
    }
    return -1;
}

static int find_contiguous_free_pages(uint32_t pages_needed) {
    for (uint32_t i = 0; i < vmm_max_pages; i++) {
        if (is_page_free(i)) {
            uint32_t j;
            for (j = 0; j < pages_needed; j++) {
                if (i + j >= vmm_max_pages || !is_page_free(i + j)) break;
            }
            if (j == pages_needed) return i;
        }
    }
    return -1;
}

// static void set_page_used(uint32_t page) {
//     uint32_t byte_idx = page / 8;
//     uint32_t bit_idx = page % 8;
//     vmm_bitmap[byte_idx] |= (1 << bit_idx);
// }

// static void set_page_free(uint32_t page) {
//     uint32_t byte_idx = page / 8;
//     uint32_t bit_idx = page % 8;
//     vmm_bitmap[byte_idx] &= ~(1 << bit_idx);
// }


static void mark_page(uint32_t page, bool used) {
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    if (used) vmm_bitmap[byte_idx] |= (1 << bit_idx);
    else vmm_bitmap[byte_idx] &= ~(1 << bit_idx);
}

void list_available_pages() {
    serial_printf("VMM: Available pages:\n");
    for (int i = 0; i < vmm_max_pages; i++) {
        if (vmm_bitmap[i] == 0) {
            serial_printf("Page %d is free\n", i);
        }
    }
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

    // list_available_pages();

}

uint32_t virt_to_phys(void *virt_addr) {
    uint32_t *pd = get_page_directory();
    uint32_t pd_index = (uint32_t)virt_addr >> 22;
    uint32_t pt_index = ((uint32_t)virt_addr >> 12) & 0x3FF;

    if (pd[pd_index] & PAGE_PRESENT) {
        uint32_t *pt = (uint32_t*)((pd[pd_index] & ~0xFFF) + KERNEL_VMEM_START);
        if (pt[pt_index] & PAGE_PRESENT) {
            return (pt[pt_index] & ~0xFFF) | ((uint32_t)virt_addr & 0xFFF);
        }
    }
    serial_printf("virt_to_phys(0x%x) failed!\n", (uint32_t)virt_addr);
    return 0;
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
    mark_page(page_index, true); // <-- Replaced set_page_used(page_index)
    uint32_t virt_addr = KERNEL_VMEM_START + page_index * PAGE_SIZE;
    
    // Allocate a physical page for mapping
    uint32_t phys_addr = (uint32_t)pmm_alloc_block();
    if (!phys_addr) {
        serial_printf("VMM: Failed to allocate physical page\n");
        mark_page(page_index, false); // <-- Replaced set_page_free(page_index)
        return 0;
    }
    
    // Map the physical page to the virtual address
    paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE);
    return (void *)virt_addr;
}

void vmm_free_page(void *addr) {
    // Check for null pointer
    if (!addr) {
        serial_printf("VMM: Attempted to free NULL address\n");
        return;
    }

    uint32_t virt_addr = (uint32_t)addr;

    // Check if address is page-aligned
    if (virt_addr & (PAGE_SIZE - 1)) {
        serial_printf("VMM: Address 0x%x is not page-aligned\n", virt_addr);
        return;
    }

    // Check if address is in valid kernel virtual memory range
    if (virt_addr < KERNEL_VMEM_START) {
        serial_printf("VMM: Invalid virtual address 0x%x\n", virt_addr);
        return;
    }

    // Calculate page index
    int page_index = (virt_addr - KERNEL_VMEM_START) / PAGE_SIZE;

    // Check if page index is within bounds
    if (page_index >= vmm_max_pages) {
        serial_printf("VMM: Page index %d out of bounds\n", page_index);
        return;
    }

    // Free the physical memory
    uint32_t phys_addr = virt_to_phys(addr);
    if (phys_addr != 0) {
        pmm_free_block((void*)phys_addr);
    }

    // Unmap the page
    paging_unmap_page(virt_addr);

    // Mark the virtual page as free
    mark_page(page_index, false);
}

void* vmm_map_mmio(uintptr_t phys_addr, size_t size, uint32_t flags) {
    // Ensure size is page-aligned
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    int pages_needed = size / PAGE_SIZE;

    // Find contiguous free virtual pages
    int start_index = -1;
    for (uint32_t i = 0; i < vmm_max_pages; i++) {
        if (is_page_free(i)) {
            uint32_t j;
            for (j = 0; j < pages_needed; j++) {
                if (i + j >= vmm_max_pages || !is_page_free(i + j)) break;
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
        mark_page(i, true);
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

void* vmm_alloc_contiguous(size_t pages) {
    if (pages == 0 || pages > vmm_max_pages) {
        serial_printf("VMM: Invalid page count %d\n", pages);
        return NULL;
    }

    // Find contiguous virtual pages
    int start_index = find_contiguous_free_pages(pages);
    if (start_index == -1) {
        serial_printf("VMM: No %d contiguous VIRTUAL pages available\n", pages);
        return NULL;
    }

    // Reserve virtual address space first
    for (int i = start_index; i < start_index + pages; i++) {
        mark_page(i, true);
    }

    // Allocate contiguous PHYSICAL memory
    void* phys_start = pmm_alloc_blocks(pages);
    if (!phys_start) {
        // Rollback virtual reservation
        for (int i = start_index; i < start_index + pages; i++) {
            mark_page(i, false);
        }
        serial_printf("VMM: PMM failed\n");
        return NULL;
        // ... error handling ...
    }

    // Map contiguous physical to contiguous virtual
    uint32_t virt_start = KERNEL_VMEM_START + start_index * PAGE_SIZE;
    for (size_t i = 0; i < pages; i++) {
        uint32_t phys_addr = (uint32_t)phys_start + (i * PAGE_SIZE);
        uint32_t virt_addr = virt_start + (i * PAGE_SIZE);
        paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    serial_printf("VMM: Allocated %d pages V:0x%x P:0x%x\n", 
                 pages, virt_start, phys_start);
    return (void*)virt_start;
}

void vmm_free_contiguous(void* virt_addr, size_t pages) {
    // Check if the virtual address is valid and the number of pages is greater than 0
    if (!virt_addr || pages == 0)
    {
        serial_printf("VMM: Invalid virtual address or page count\n");
        return;
    } 

    // Get the starting physical address from the first page
    uint32_t phys_start = virt_to_phys(virt_addr);
    if (phys_start == UINT32_MAX) {
        serial_printf("VMM: Failed to get physical address for virtual address 0x%x\n", virt_addr);
        return;
    }
    
    // Free the physical memory as a single block
    pmm_free_blocks((void*)phys_start, pages);

    // Unmap and free the virtual pages
    uint32_t virt_start = (uint32_t)virt_addr;
    int start_index = (virt_start - KERNEL_VMEM_START) / PAGE_SIZE;
    
    for (size_t i = 0; i < pages; i++) {
        // Unmap the page from the virtual address space
        paging_unmap_page(virt_start + (i * PAGE_SIZE));
        // Mark the page as free in the bitmap
        mark_page(start_index + i, false);
    }
}

/**
 * Allocates contiguous physical pages for DMA.
 * 
 * @param size The size in bytes to allocate.
 * @return A pointer to the allocated virtual address, or NULL on failure.
 */
void* dma_alloc(size_t size) {
    // Restrict DMA to first 16MB to avoid VESA region
    uint32_t dma_zone_start = 0x100000; 
    uint32_t dma_zone_end = 0x1000000; // 16MB
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Find contiguous physical memory below 16MB
    void* phys = pmm_alloc_blocks_in_range(pages, dma_zone_start, dma_zone_end);
    if (!phys) return NULL;

    // Map to virtual memory
    void* virt = vmm_alloc_contiguous(pages);
    for (uint32_t i = 0; i < pages; i++) {
        paging_map_page(
            (uint32_t)phys + i * PAGE_SIZE,
            (uint32_t)virt + i * PAGE_SIZE,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_UNCACHED // Critical for DMA
        );
    }
    return virt;
}

void dma_free(void* addr, size_t size) {
    // Check for null pointer and size
    if (!addr || size == 0) {
        serial_printf("DMA: Invalid address or size\n");
        return;
    }

    // Check alignment
    if (((uintptr_t)addr & (PAGE_SIZE - 1)) != 0) {
        serial_printf("DMA: Address not page aligned\n");
        return;
    }

    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Check for overflow
    if (pages > vmm_max_pages) {
        serial_printf("DMA: Size too large\n");
        return;
    }

    vmm_free_contiguous(addr, pages);
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt_addr = (uint32_t)addr + i * PAGE_SIZE;
        paging_unmap_page(virt_addr);
    }
}