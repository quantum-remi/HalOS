#ifndef PMM_H
#define PMM_H

#include "types.h"
#include "string.h"

#define PMM_BLOCK_SIZE 4096
#define PMM_BLOCKS_PER_BYTE 8

void pmm_init(size_t mem_size, uint32 *bitmap);
void *pmm_alloc_block();
void *pmm_alloc_blocks(int num_blocks);
void pmm_free_block(void *p);

#endif
