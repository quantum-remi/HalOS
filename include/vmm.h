#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_VMEM_START 0xC0000000
extern uint32_t vmm_max_pages; 

// Initialize the Virtual Memory Manager and enable paging support.
void vmm_init();
// Allocate one virtual page and map it to a physical page.
void *vmm_alloc_page();
// Free the virtual page at the given address.
void vmm_free_page(void *addr);

void* vmm_map_mmio(uintptr_t phys_addr, size_t size, uint32_t flags);

uint32_t virt_to_phys(void *virt_addr);

uint32_t phys_to_virt(uint32_t phys_addr);

void* vmm_alloc_contiguous(size_t pages);

void vmm_free_contiguous(void* addr, size_t pages);

void* dma_alloc(size_t size);

void dma_free(void* addr, size_t size);

#endif
