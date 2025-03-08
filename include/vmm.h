#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_VMEM_START 0xC0000000
#define VMM_MAX_PAGES     1024

// Initialize the Virtual Memory Manager and enable paging support.
void vmm_init();
// Allocate one virtual page and map it to a physical page.
void *vmm_alloc_page();
// Free the virtual page at the given address.
void vmm_free_page(void *addr);

#endif
