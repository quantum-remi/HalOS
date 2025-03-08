// dma.c
#include "dma.h"
#include "pmm.h" // Physical Memory Manager
#include "string.h"

// Assume PAGE_SIZE = 4096 and alignment requirements are met by page boundaries
void *dma_alloc(size_t size, uint32_t *phys_addr)
{
    // Calculate number of pages needed
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Allocate contiguous physical pages
    uint32_t phys = pmm_alloc_contiguous(pages);
    if (!phys)
        return NULL;

    // Convert to virtual address (identity-mapped in protected mode)
    void *virt = (void *)(uintptr_t)phys;

    // Clear the allocated memory
    memset(virt, 0, pages * PAGE_SIZE);

    *phys_addr = phys;
    return virt;
}

void dma_free(void *virt, size_t size)
{
    // Convert virtual to physical (identity mapping)
    uint32_t phys = (uint32_t)(uintptr_t)virt;

    // Calculate pages to free
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Release physical pages
    pmm_free_contiguous(phys, pages);
}