#include "pmm.h"
#include "serial.h"
#include "string.h"
#include <stdbool.h>

#define PMM_DEBUG 1
#define PMM_ERROR(msg, ...) serial_printf("PMM ERROR: " msg "\n", ##__VA_ARGS__)
#define PMM_LOG(msg, ...) if(PMM_DEBUG) serial_printf("PMM: " msg "\n", ##__VA_ARGS__)

static uint32_t pmm_memory_size = 0;
uint32_t pmm_used_blocks = 0;
static uint32_t pmm_max_blocks = 0;
static uint8_t *pmm_memory_map = 0;
static bool pmm_initialized = false;
static uint32_t pmm_last_alloc = 0;

void pmm_init(size_t mem_size, uint8_t *bitmap)
{
    if (mem_size == 0 || bitmap == NULL)
    {
        serial_printf("PMM: Invalid parameters\n");
        return;
    }

    pmm_memory_size = mem_size;
    pmm_memory_map = bitmap;
    pmm_max_blocks = (mem_size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;
    pmm_used_blocks = 0;

    memset(pmm_memory_map, 0x00, pmm_max_blocks / PMM_BLOCKS_PER_BYTE);

    pmm_mark_used_region(0, 0x100000);

    uint32_t kernel_start = (uint32_t)&__kernel_physical_start;
    uint32_t kernel_end = (uint32_t)&__kernel_physical_end;
    uint32_t kernel_size = kernel_end - kernel_start;

    serial_printf("PMM: Kernel physical start: 0x%x, end: 0x%x, size: %d bytes\n",
                  kernel_start, kernel_end, kernel_size);

    pmm_mark_used_region(kernel_start, kernel_size);

    pmm_mark_used_region((uint32_t)bitmap, pmm_max_blocks / PMM_BLOCKS_PER_BYTE);

    serial_printf("PMM: Initialized with %d blocks (%.2f MB)\n",
                  pmm_max_blocks, (float)pmm_memory_size / (1024 * 1024));
    serial_printf("PMM: Used blocks: %d\n", pmm_used_blocks);
    serial_printf("PMM: Bitmap at 0x%x\n", (uint32_t)bitmap);
    serial_printf("PMM: Free blocks: %d\n", pmm_max_blocks - pmm_used_blocks);

    pmm_initialized = true;
}

uint32_t pmm_get_total_memory()
{
    return pmm_memory_size;
}

void pmm_mark_used_region(uint32_t base, uint32_t size)
{
    uint32_t start_block = base / PMM_BLOCK_SIZE;
    uint32_t num_blocks = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    for (uint32_t i = 0; i < num_blocks; i++)
    {
        uint32_t block = start_block + i;
        if (block >= pmm_max_blocks)
            break;

        uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
        uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

        if (!(pmm_memory_map[byte_idx] & (1 << bit_idx)))
        {
            pmm_memory_map[byte_idx] |= (1 << bit_idx);
            pmm_used_blocks++;
        }
    }
}

void pmm_mark_unused_region(uint32_t base, uint32_t size)
{
    uint32_t start_block = base / PMM_BLOCK_SIZE;
    uint32_t num_blocks = (size + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    for (uint32_t i = 0; i < num_blocks; i++)
    {
        uint32_t block = start_block + i;
        if (block >= pmm_max_blocks)
            break;

        uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
        uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

        if (pmm_memory_map[byte_idx] & (1 << bit_idx))
        {
            pmm_memory_map[byte_idx] &= ~(1 << bit_idx);
            pmm_used_blocks--;
        }
    }
}

void *pmm_alloc_block()
{
    if (pmm_used_blocks >= pmm_max_blocks)
    {
        serial_printf("PMM: No free blocks available\n");
        return NULL;
    }

    for (uint32_t byte_idx = 0; byte_idx < pmm_max_blocks / PMM_BLOCKS_PER_BYTE; byte_idx++)
    {
        if (pmm_memory_map[byte_idx] == 0xFF)
            continue;

        for (uint32_t bit_idx = 0; bit_idx < PMM_BLOCKS_PER_BYTE; bit_idx++)
        {
            uint32_t block = byte_idx * PMM_BLOCKS_PER_BYTE + bit_idx;
            if (block >= pmm_max_blocks)
                break;

            if (!(pmm_memory_map[byte_idx] & (1 << bit_idx)))
            {
                pmm_memory_map[byte_idx] |= (1 << bit_idx);
                pmm_used_blocks++;
                return (void *)(block * PMM_BLOCK_SIZE);
            }
        }
    }
    serial_printf("PMM: Out of memory!\n");
    return NULL;
}

static bool validate_allocation_request(size_t blocks) {
    if (!pmm_initialized) {
        PMM_ERROR("PMM not initialized");
        return false;
    }
    if (blocks == 0 || blocks > pmm_max_blocks) {
        PMM_ERROR("Invalid block count: %d", blocks);
        return false;
    }
    if (pmm_used_blocks + blocks > pmm_max_blocks) {
        PMM_ERROR("Out of memory: requested %d blocks", blocks);
        return false;
    }
    return true;
}

void *pmm_alloc_blocks(uint32_t num_blocks)
{
    if (!validate_allocation_request(num_blocks)) {
        return NULL;
    }

    uint32_t consecutive = 0;
    uint32_t start_block = 0;

    uint32_t start = pmm_last_alloc;
    uint32_t tries = 0;
    const uint32_t max_tries = 2;

    while (tries < max_tries) {
        for (uint32_t block = start; block < pmm_max_blocks; block++) {
            uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
            uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

            if (!(pmm_memory_map[byte_idx] & (1 << bit_idx))) {
                if (consecutive == 0)
                    start_block = block;
                consecutive++;
                if (consecutive == num_blocks) {
                    for (uint32_t i = start_block; i < start_block + num_blocks; i++) {
                        uint32_t b_idx = i / PMM_BLOCKS_PER_BYTE;
                        uint32_t bit = i % PMM_BLOCKS_PER_BYTE;
                        pmm_memory_map[b_idx] |= (1 << bit);
                        pmm_used_blocks++;
                    }
                    pmm_last_alloc = (start_block + num_blocks) % pmm_max_blocks;
                    return (void *)(start_block * PMM_BLOCK_SIZE);
                }
            } else {
                consecutive = 0;
            }
        }
        start = 0;
        tries++;
    }

    PMM_ERROR("Failed to find %d contiguous blocks", num_blocks);
    return NULL;
}

void pmm_free_block(void *p)
{
    if (p == NULL)
        serial_printf("PMM: Invalid free address (NULL)\n");
    return;

    uint32_t addr = (uint32_t)p;
    if (addr % PMM_BLOCK_SIZE != 0)
    {
        serial_printf("PMM: Invalid free address 0x%x (not aligned)\n", addr);
        return;
    }

    uint32_t block = addr / PMM_BLOCK_SIZE;

    if (block >= pmm_max_blocks)
    {
        serial_printf("PMM: Invalid free address 0x%x (out of bounds)\n", addr);
        return;
    }

    uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
    uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;

    if (pmm_memory_map[byte_idx] & (1 << bit_idx))
    {
        pmm_memory_map[byte_idx] &= ~(1 << bit_idx);
        pmm_used_blocks--;
        serial_printf("PMM: Freed block at physical address 0x%x\n", addr);
    }
    else
    {
        serial_printf("PMM: Double-free at 0x%x\n", addr);
    }
}

void pmm_free_blocks(void *p, int num_blocks)
{
    if (!p || num_blocks <= 0)
        return;

    uint32_t addr = (uint32_t)p;
    uint32_t start_block = addr / PMM_BLOCK_SIZE;

    if (start_block + num_blocks > pmm_max_blocks)
    {
        serial_printf("PMM: Invalid free request for %d blocks starting at 0x%x\n", num_blocks, addr);
        return;
    }

    for (int i = 0; i < num_blocks; i++)
    {
        pmm_free_block((char *)p + i * PMM_BLOCK_SIZE);
    }
}

bool pmm_is_block_free(uint32_t block)
{
    if (block >= pmm_max_blocks)
    {
        return false;
    }
    uint32_t byte_idx = block / PMM_BLOCKS_PER_BYTE;
    uint32_t bit_idx = block % PMM_BLOCKS_PER_BYTE;
    return !(pmm_memory_map[byte_idx] & (1 << bit_idx));
}

void *pmm_alloc_blocks_in_range(uint32_t num_blocks, uint32_t start_addr, uint32_t end_addr)
{
    if (num_blocks == 0 || start_addr >= end_addr)
    {
        serial_printf("PMM: Invalid parameters for block allocation in range\n");
        return NULL;
    }

    uint32_t block_size = PMM_BLOCK_SIZE;
    uint32_t start_block = start_addr / block_size;
    uint32_t end_block = end_addr / block_size;

    uint32_t consecutive = 0;
    for (uint32_t block = start_block; block < end_block; block++)
    {
        if (pmm_is_block_free(block))
        {
            consecutive++;
            if (consecutive == num_blocks)
            {
                uint32_t first_block = block - num_blocks + 1;
                pmm_mark_used_region(first_block * block_size, num_blocks * block_size);
                return (void *)(first_block * block_size);
            }
        }
        else
        {
            consecutive = 0;
        }
    }
    serial_printf("PMM: No contiguous blocks available in range 0x%x - 0x%x for %d blocks\n", start_addr, end_addr, num_blocks);
    return NULL;
}
