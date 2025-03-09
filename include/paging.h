#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE     4096
#define PAGE_PRESENT  0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER     0x4

#define PAGE_UNCACHED  (1 << 4)  // PCD (Page Cache Disable) flag

// Initialize paging system (allocates the page directory)
void paging_init(uint32_t mem_size);
// Enable paging by loading CR3 and setting CR0.PG
void paging_enable(uint32_t page_directory);
// Map a physical page to a virtual address with given flags (creates page table if needed)
void paging_map_page(uint32_t phys_addr, uint32_t virt_addr, uint32_t flags);

// Get the current page directory address
uint32_t *get_page_directory(void);

#endif
