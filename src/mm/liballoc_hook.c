#include "liballoc_hook.h"

int liballoc_lock()
{
    __asm__("cli");
    return 0;
}

int liballoc_unlock()
{
    __asm__("sti");
    return 0;
}

void *liballoc_alloc(int num_blocks)
{
    if (num_blocks <= 0)
        return NULL;

    // Allocate a contiguous region of physical memory blocks.
    return pmm_alloc_blocks(num_blocks);
}

int liballoc_free(void *ptr, int num_blocks)
{
    if (!ptr)
        return -1; // Return error if the pointer is NULL

    // Free the contiguous region of blocks.
    pmm_free_blocks(ptr, num_blocks);
    return 0;
}
