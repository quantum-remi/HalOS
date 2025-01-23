#include "pmm.h"
#include "serial.h"
#include "kernel.h"

extern uint32 __kernel_physical_start;
extern uint32 __kernel_virtual_start;

static uint32 pmm_memory_size = 0;
static uint32 pmm_used_blocks = 0;
static uint32 pmm_max_blocks = 0;
static uint32 *pmm_memory_map = 0;

void pmm_init(size_t mem_size, uint32 *bitmap) {
    if (mem_size == 0 || bitmap == NULL) {
        serial_printf("PMM: Invalid memory size or bitmap.\n");
        return;
    }

    pmm_memory_size = mem_size;
    pmm_memory_map = bitmap;
    pmm_max_blocks = (pmm_memory_size * 1024) / PMM_BLOCK_SIZE;
    pmm_used_blocks = pmm_max_blocks;

    // Mark all memory as used initially
    memset(pmm_memory_map, 0xFF, pmm_max_blocks / PMM_BLOCKS_PER_BYTE);
    serial_printf("PMM initialized with %d blocks.\n", pmm_max_blocks);
}

void *pmm_alloc_block() {
    if (pmm_memory_map == NULL) {
        serial_printf("PMM: Memory map not initialized.\n");
        return 0;
    }

    for (uint32 i = 0; i < pmm_max_blocks / PMM_BLOCKS_PER_BYTE; i++) {
        if (pmm_memory_map[i] != 0xFF) {
            for (uint32 j = 0; j < PMM_BLOCKS_PER_BYTE; j++) {
                uint32 bit = 1 << j;
                if (!(pmm_memory_map[i] & bit)) {
                    pmm_memory_map[i] |= bit;
                    pmm_used_blocks++;
                    return (void *)((i * PMM_BLOCKS_PER_BYTE + j) * PMM_BLOCK_SIZE + __kernel_physical_start);
                }
            }
        }
    }
    serial_printf("PMM: No free blocks available.\n");
    return 0; // No free blocks
}

void pmm_free_block(void *p) {
    if (pmm_memory_map == NULL) {
        serial_printf("PMM: Memory map not initialized.\n");
        return;
    }

    uint32 addr = (uint32)p - __kernel_physical_start;
    uint32 block = addr / PMM_BLOCK_SIZE;
    uint32 i = block / PMM_BLOCKS_PER_BYTE;
    uint32 j = block % PMM_BLOCKS_PER_BYTE;
    if (pmm_memory_map[i] & (1 << j)) {
        pmm_memory_map[i] &= ~(1 << j);
        pmm_used_blocks--;
    } else {
        serial_printf("PMM: Attempt to free an unallocated block at address 0x%x.\n", addr);
    }
}