#include "pmm.h"
#include "serial.h"
#include "string.h"

static uint32_t pmm_memory_size = 0;
static uint32_t pmm_used_blocks = 0;
static uint32_t pmm_max_blocks = 0;
static uint32_t *pmm_memory_map = 0;

// Spinlock for thread safety (if multi-threaded)
// static spinlock_t pmm_lock = SPINLOCK_INIT;

// Initialize PMM with memory size (in bytes) and pre-allocated bitmap
void pmm_init(size_t mem_size, uint32_t *bitmap) {
    if (mem_size == 0 || bitmap == NULL) {
        serial_printf("PMM: Invalid parameters\n");
        return;
    }

    pmm_memory_size = mem_size;
    pmm_memory_map = bitmap;
    pmm_max_blocks = (mem_size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE; // Round up
    pmm_used_blocks = 0;

    // Mark all blocks as free initially
    memset(pmm_memory_map, 0x00, pmm_max_blocks / PMM_BLOCKS_PER_BYTE);

    // Mark kernel memory as used
    uint32_t kernel_size = __kernel_physical_end - __kernel_physical_start;
    pmm_mark_used_region(__kernel_physical_start, kernel_size);

    serial_printf("PMM: Initialized with %d blocks (%.2f MB)\n", 
        pmm_max_blocks, (float)pmm_memory_size / (1024 * 1024));
}

// Mark a physical memory region as used
void pmm_mark_used_region(uint32_t base, uint32_t size) {
    uint32_t start_block = base / PMM_BLOCK_SIZE;
    uint32_t num_blocks = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE; // Round up

    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block = start_block + i;
        if (block >= pmm_max_blocks) break;

        uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
        uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

        if (!(pmm_memory_map[byte_idx] & (1 << bit_idx))) {
            pmm_memory_map[byte_idx] |= (1 << bit_idx);
            pmm_used_blocks++;
        }
    }
}

// Allocate a single 4KB block
void *pmm_alloc_block() {
    // spin_lock(&pmm_lock);

    for (uint32_t byte_idx = 0; byte_idx < pmm_max_blocks / PMM_BLOCKS_PER_BYTE; byte_idx++) {
        if (pmm_memory_map[byte_idx] == 0xFF) continue; // All blocks in this byte are used

        for (uint32_t bit_idx = 0; bit_idx < PMM_BLOCKS_PER_BYTE; bit_idx++) {
            uint32_t block = byte_idx * PMM_BLOCKS_PER_BYTE + bit_idx;
            if (block >= pmm_max_blocks) break;

            if (!(pmm_memory_map[byte_idx] & (1 << bit_idx))) {
                pmm_memory_map[byte_idx] |= (1 << bit_idx);
                pmm_used_blocks++;
                // spin_unlock(&pmm_lock);
                return (void *)(block * PMM_BLOCK_SIZE); // Return physical address
            }
        }
    }

    // spin_unlock(&pmm_lock);
    serial_printf("PMM: Out of memory!\n");
    return NULL;
}

// Free a block
void pmm_free_block(void *p) {
    if (p == NULL) return;

    uint32_t addr = (uint32_t)p;
    uint32_t block = addr / PMM_BLOCK_SIZE;

    if (block >= pmm_max_blocks) {
        serial_printf("PMM: Invalid free address 0x%x\n", addr);
        return;
    }

    uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
    uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

    // spin_lock(&pmm_lock);
    if (pmm_memory_map[byte_idx] & (1 << bit_idx)) {
        pmm_memory_map[byte_idx] &= ~(1 << bit_idx);
        pmm_used_blocks--;
    } else {
        serial_printf("PMM: Double-free at 0x%x\n", addr);
    }
    // spin_unlock(&pmm_lock);
}