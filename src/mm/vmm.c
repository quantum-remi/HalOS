#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include <stdbool.h>

extern uint32_t __kernel_vmem_start;

uint8_t *vmm_bitmap = NULL;
uint32_t vmm_max_pages = 0;

static bool is_page_free(uint32_t page)
{
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    return !(vmm_bitmap[byte_idx] & (1 << bit_idx));
}

static void vmm_bitmap_init()
{
    uint32_t total_phys_mem = pmm_get_total_memory();
    if (total_phys_mem == 0)
    {
        serial_printf("VMM: Total physical memory is zero!\n");
        return;
    }

    vmm_max_pages = total_phys_mem / PAGE_SIZE;
    uint32_t bitmap_size = (vmm_max_pages + 7) / 8;
    uint32_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    void *bitmap_phys = pmm_alloc_blocks(bitmap_pages);
    if (!bitmap_phys)
    {
        serial_printf("VMM: Failed to allocate bitmap physical memory\n");
        return;
    }

    vmm_bitmap = vmm_map_mmio((uintptr_t)bitmap_phys, bitmap_pages * PAGE_SIZE, PAGE_PRESENT | PAGE_WRITABLE);
    if (!vmm_bitmap)
    {
        serial_printf("VMM: Failed to map bitmap to virtual memory\n");
        pmm_free_blocks(bitmap_phys, bitmap_pages);
        return;
    }

    memset(vmm_bitmap, 0, bitmap_size);
    serial_printf("VMM: Bitmap initialized with %u pages\n", vmm_max_pages);
}

static int find_free_page()
{
    for (uint32_t i = 0; i < vmm_max_pages; i++)
    {
        uint32_t byte_idx = i / 8;
        uint32_t bit_idx = i % 8;
        if (!(vmm_bitmap[byte_idx] & (1 << bit_idx)))
        {
            return i;
        }
    }
    return -1;
}

static int find_contiguous_free_pages(uint32_t pages_needed)
{
    for (uint32_t i = 0; i < vmm_max_pages; i++)
    {
        if (is_page_free(i))
        {
            uint32_t j;
            for (j = 0; j < pages_needed; j++)
            {
                if (i + j >= vmm_max_pages || !is_page_free(i + j))
                    break;
            }
            if (j == pages_needed)
                return i;
        }
    }
    return -1;
}

static void mark_page(uint32_t page, bool used)
{
    uint32_t byte_idx = page / 8;
    uint32_t bit_idx = page % 8;
    if (used)
        vmm_bitmap[byte_idx] |= (1 << bit_idx);
    else
        vmm_bitmap[byte_idx] &= ~(1 << bit_idx);
}

void list_available_pages(void)
{
    serial_printf("VMM: Available pages:\n");
    for (uint32_t i = 0; i < vmm_max_pages; i++)
    {
        if (vmm_bitmap[i] == 0)
        {
            serial_printf("Page %d is free\n", i);
        }
    }
}

void vmm_init()
{
    paging_init();
    vmm_bitmap_init();

    uint32_t *pd = get_page_directory();
    if (!pd)
    {
        serial_printf("VMM: Failed to get page directory!\n");
        return;
    }

    extern uint32_t __kernel_physical_start;
    extern uint32_t __kernel_physical_end;
    uint32_t kernel_size = (uint32_t)&__kernel_physical_end - (uint32_t)&__kernel_physical_start;
    uint32_t num_pages = (kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t i = 0; i < num_pages; i++)
    {
        uint32_t phys_addr = (uint32_t)&__kernel_physical_start + (i * PAGE_SIZE);
        uint32_t virt_addr = KERNEL_VMEM_START + (i * PAGE_SIZE);
        paging_map_page(phys_addr, phys_addr, PAGE_PRESENT | PAGE_WRITABLE);
        paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    paging_enable((uint32_t)pd);

    __asm__ volatile(
        "mov $0x10, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "mov %ax, %ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n");
}

uint32_t virt_to_phys(void *virt_addr)
{
    uint32_t *pd = get_page_directory();
    uint32_t pd_index = (uint32_t)virt_addr >> 22;
    uint32_t pt_index = ((uint32_t)virt_addr >> 12) & 0x3FF;
    
    if (!(pd[pd_index] & PAGE_PRESENT))
        return UINT32_MAX;
        
    uint32_t pt_phys = pd[pd_index] & ~0xFFF;
    
    uint32_t *pt;
    if (paging_active) {
        pt = (uint32_t *)(0xC0000000 + pt_phys);
    } else {
        pt = (uint32_t *)pt_phys;
    }
    
    if (!(pt[pt_index] & PAGE_PRESENT))
        return UINT32_MAX;
        
    uint32_t page_phys = pt[pt_index] & ~0xFFF;
    return page_phys | ((uint32_t)virt_addr & 0xFFF);
}

uint32_t phys_to_virt(uint32_t phys_addr)
{
    return phys_addr + KERNEL_VMEM_START;
}

void *vmm_alloc_page()
{
    int page_index = find_free_page();
    if (page_index < 0)
    {
        serial_printf("VMM: No free virtual pages available\n");
        return NULL;
    }

    mark_page(page_index, true);
    uint32_t virt_addr = KERNEL_VMEM_START + page_index * PAGE_SIZE;

    void *phys_ptr = pmm_alloc_block();
    if (!phys_ptr)
    {
        serial_printf("VMM: Failed to allocate physical page\n");
        mark_page(page_index, false);
        return NULL;
    }
    uint32_t phys_addr = (uint32_t)phys_ptr;

    if (!paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE))
    {
        serial_printf("VMM: Failed to map physical page to virtual address\n");
        pmm_free_block(phys_ptr);
        mark_page(page_index, false);
        return NULL;
    }

    return (void *)virt_addr;
}

void vmm_free_page(void *addr)
{
    if (!addr)
    {
        serial_printf("VMM: Attempted to free NULL address\n");
        return;
    }

    uint32_t virt_addr = (uint32_t)addr;

    if (virt_addr & (PAGE_SIZE - 1))
    {
        serial_printf("VMM: Address 0x%x is not page-aligned\n", virt_addr);
        return;
    }

    if (virt_addr < KERNEL_VMEM_START)
    {
        serial_printf("VMM: Invalid virtual address 0x%x\n", virt_addr);
        return;
    }

    uint32_t page_index = (virt_addr - KERNEL_VMEM_START) / PAGE_SIZE;

    if (page_index >= vmm_max_pages)
    {
        serial_printf("VMM: Page index %u out of bounds\n", page_index);
        return;
    }

    uint32_t phys_addr = virt_to_phys(addr);
    if (phys_addr == UINT32_MAX)
    {
        serial_printf("VMM: Failed to resolve physical address for 0x%x\n", virt_addr);
        return;
    }

    pmm_free_block((void *)phys_addr);
    paging_unmap_page(virt_addr);
    mark_page(page_index, false);
    serial_printf("VMM: Freed page at V:0x%x P:0x%x\n", virt_addr, phys_addr);
}

void *vmm_map_mmio(uintptr_t phys_addr, size_t size, uint32_t flags)
{
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    int start_index = -1;
    for (uint32_t i = 0; i < vmm_max_pages; i++)
    {
        if (is_page_free(i))
        {
            uint32_t j;
            for (j = 0; j < pages_needed; j++)
            {
                if (i + j >= vmm_max_pages || !is_page_free(i + j))
                    break;
            }
            if (j == pages_needed)
            {
                start_index = i;
                break;
            }
        }
    }

    if (start_index < 0)
    {
        serial_printf("VMM: Not enough contiguous virtual space for MMIO\n");
        return NULL;
    }

    for (uint32_t i = start_index; i < start_index + pages_needed; i++)
    {
        mark_page(i, true);
    }

    uint32_t virt_start = KERNEL_VMEM_START + start_index * PAGE_SIZE;
    for (uint32_t i = 0; i < pages_needed; i++) {
        uintptr_t curr_phys = phys_addr + (i * PAGE_SIZE);
        uintptr_t curr_virt = virt_start + (i * PAGE_SIZE);
        
        if (!paging_map_page(curr_phys, curr_virt, flags | PAGE_PRESENT | PAGE_WRITABLE)) {
            serial_printf("VMM: Failed to map MMIO page at V:0x%x P:0x%x\n", curr_virt, curr_phys);
            return NULL;
        }
    }

    return (void *)virt_start;
}

void *vmm_alloc_contiguous(size_t pages)
{
    if (pages == 0 || pages > vmm_max_pages)
    {
        serial_printf("VMM: Invalid page count %d\n", pages);
        return NULL;
    }
    
    int start_index = find_contiguous_free_pages(pages);
    if (start_index == -1)
    {
        serial_printf("VMM: No %d contiguous virtual pages available\n", pages);
        return NULL;
    }
    
    for (uint32_t i = start_index; i < (uint32_t)(start_index + pages); i++)
    {
        mark_page(i, true);
    }
    
    void *phys_ptr = pmm_alloc_contiguous(pages);
    if (!phys_ptr)
    {
        for (uint32_t i = start_index; i < (uint32_t)(start_index + pages); i++)
        {
            mark_page(i, false);
        }
        serial_printf("VMM: Failed to allocate contiguous physical memory\n");
        return NULL;
    }
    
    uintptr_t phys_start = (uintptr_t)phys_ptr;
    
    uintptr_t virt_start = KERNEL_VMEM_START + start_index * PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++)
    {
        uintptr_t phys_addr = phys_start + (i * PAGE_SIZE);
        uintptr_t virt_addr = virt_start + (i * PAGE_SIZE);
        if (!paging_map_page(phys_addr, virt_addr, PAGE_PRESENT | PAGE_WRITABLE))
        {
            serial_printf("VMM: Failed to map page %d\n", i);
            vmm_free_contiguous((void *)virt_start, i);
            return NULL;
        }
    }
    
    serial_printf("VMM: Allocated %d contiguous pages V:0x%x P:0x%x\n", pages, virt_start, phys_start);
    return (void *)virt_start;
}

void vmm_free_contiguous(void *virt_addr, size_t pages)
{
    if (!virt_addr || pages == 0)
    {
        serial_printf("VMM: Invalid virtual address or page count\n");
        return;
    }

    uintptr_t phys_start = virt_to_phys(virt_addr);
    if (phys_start == UINT32_MAX)
    {
        serial_printf("VMM: Failed to get physical address for virtual address 0x%x\n", virt_addr);
        return;
    }

    serial_printf("VMM: Freeing %d pages at V:0x%x P:0x%x\n", pages, (uintptr_t)virt_addr, phys_start);
    pmm_free_blocks((void *)phys_start, pages);

    uintptr_t virt_start = (uintptr_t)virt_addr;
    int start_index = (virt_start - KERNEL_VMEM_START) / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++)
    {
        paging_unmap_page(virt_start + (i * PAGE_SIZE));
        pmm_mark_unused_region(phys_start + (i * PAGE_SIZE), PAGE_SIZE);
        mark_page(start_index + i, false);
    }
}

void *dma_alloc(size_t size)
{
    uintptr_t dma_zone_start = 0x100000;
    uintptr_t dma_zone_end = 0x1000000;
    uintptr_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    void *phys = pmm_alloc_blocks_in_range(pages, dma_zone_start, dma_zone_end);
    if (!phys)
        return NULL;

    void *virt = vmm_alloc_contiguous(pages);
    for (uint32_t i = 0; i < pages; i++)
    {
        paging_map_page(
            (uintptr_t)phys + i * PAGE_SIZE,
            (uintptr_t)virt + i * PAGE_SIZE,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_UNCACHED
        );
    }
    return virt;
}

void dma_free(void *addr, size_t size)
{
    if (!addr || size == 0)
    {
        serial_printf("DMA: Invalid address or size\n");
        return;
    }

    if (((uintptr_t)addr & (PAGE_SIZE - 1)) != 0)
    {
        serial_printf("DMA: Address not page aligned\n");
        return;
    }

    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (pages > vmm_max_pages)
    {
        serial_printf("DMA: Size too large\n");
        return;
    }

    serial_printf("DMA: Freeing %d pages at 0x%x\n", pages, (uintptr_t)addr);

    vmm_free_contiguous(addr, pages);
}

bool vmm_map_userspace(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    if (virt_addr >= KERNEL_VMEM_START) {
        serial_printf("VMM: Invalid userspace address 0x%x\n", virt_addr);
        return false;
    }

    flags |= PAGE_PRESENT;
    flags |= PAGE_USER;

    return paging_map_page(phys_addr, virt_addr, flags) == 1;
}

void* vmm_alloc_userspace_pages(size_t pages) {
    uint32_t virt_addr = USER_SPACE_END - pages * PAGE_SIZE;
    void* phys = pmm_alloc_blocks(pages);
    if(!phys) return NULL;

    for(uint32_t i = 0; i < pages; i++) {
        if (!paging_map_page(
            (uint32_t)phys + i * PAGE_SIZE,
            virt_addr + i * PAGE_SIZE,
            PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER
        )) {
            for (uint32_t j = 0; j < i; j++) {
                paging_unmap_page(virt_addr + j * PAGE_SIZE);
            }
            pmm_free_blocks(phys, pages);
            return NULL;
        }
    }
    return (void*)virt_addr;
}
