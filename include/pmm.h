#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PMM_BLOCK_SIZE 4096   // 4KB blocks
#define PMM_BLOCKS_PER_BYTE 8 // 8 blocks per byte (1 bit per block)

extern uint32_t pmm_used_blocks;

uint32_t pmm_get_total_memory();

bool pmm_is_block_free(uint32_t block);
void pmm_init(size_t mem_size, uint8_t *bitmap); // changed type from uint32_t* to uint8_t*
void pmm_mark_used_region(uint32_t base, uint32_t size);
void pmm_mark_unused_region(uint32_t base, uint32_t size);
void *pmm_alloc_block();
void *pmm_alloc_blocks(uint32_t num_blocks);
void pmm_free_block(void *p);
void pmm_free_blocks(void *p, int num_blocks);
void *pmm_alloc_blocks_in_range(uint32_t num_blocks, uint32_t start_addr, uint32_t end_addr);
extern uint32_t __kernel_physical_start; // Defined in linker script
extern uint32_t __kernel_physical_end;

#endif