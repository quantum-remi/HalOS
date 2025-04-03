#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KERNEL_VMEM_START 0xC0000000
#define USER_SPACE_START  0x00000000
#define USER_SPACE_END    0xBFFFFFFF

extern uint32_t vmm_max_pages; 

/**
 * Initialize the Virtual Memory Manager and enable paging support.
 */
void vmm_init();

/**
 * Allocate one virtual page and map it to a physical page.
 * @return Pointer to the allocated virtual page, or NULL on failure.
 */
void *vmm_alloc_page();

/**
 * Free the virtual page at the given address.
 * @param addr Virtual address of the page to free.
 */
void vmm_free_page(void *addr);

/**
 * Map a physical memory region to virtual memory for MMIO.
 * @param phys_addr Physical address to map.
 * @param size Size of the region in bytes.
 * @param flags Page flags for the mapping.
 * @return Pointer to the mapped virtual address, or NULL on failure.
 */
void* vmm_map_mmio(uintptr_t phys_addr, size_t size, uint32_t flags);

/**
 * Convert a virtual address to a physical address.
 * @param virt_addr Virtual address to convert.
 * @return Physical address, or UINT32_MAX on failure.
 */
uint32_t virt_to_phys(void *virt_addr);

/**
 * Convert a physical address to a virtual address.
 * @param phys_addr Physical address to convert.
 * @return Virtual address.
 */
uint32_t phys_to_virt(uint32_t phys_addr);

/**
 * Allocate contiguous virtual pages and map them to contiguous physical pages.
 * @param pages Number of pages to allocate.
 * @return Pointer to the starting virtual address, or NULL on failure.
 */
void* vmm_alloc_contiguous(size_t pages);

/**
 * Free contiguous virtual pages.
 * @param addr Starting virtual address of the pages to free.
 * @param pages Number of pages to free.
 */
void vmm_free_contiguous(void* addr, size_t pages);

/**
 * Allocate memory for DMA operations.
 * @param size Size in bytes to allocate.
 * @return Pointer to the allocated virtual address, or NULL on failure.
 */
void* dma_alloc(size_t size);

/**
 * Free memory allocated for DMA operations.
 * @param addr Virtual address of the memory to free.
 * @param size Size in bytes of the memory to free.
 */
void dma_free(void* addr, size_t size);


void* vmm_alloc_region(size_t size, uint32_t flags);

void vmm_print_stats();

#endif
