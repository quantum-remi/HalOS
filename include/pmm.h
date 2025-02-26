#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PMM_BLOCK_SIZE        4096     // 4KB blocks
#define PMM_BLOCKS_PER_BYTE   8        // 8 blocks per byte (1 bit per block)

void pmm_init(size_t mem_size, uint32_t *bitmap);
void pmm_mark_used_region(uint32_t base, uint32_t size);
void *pmm_alloc_block();
void *pmm_alloc_blocks(int num_blocks);
void pmm_free_block(void *p);
void pmm_free_blocks(void *p, int num_blocks);

extern uint32_t __kernel_physical_start; // Defined in linker script
extern uint32_t __kernel_physical_end;

#endif