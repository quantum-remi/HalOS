#include "liballoc_hook.h"

int liballoc_lock()
{
    asm("cli");
    return 0;
}

int liballoc_unlock()
{
    asm("sti");
    return 0;
}

void* liballoc_alloc(int num_blocks)
{
    // Use PMM to allocate the requested number of blocks
    void* memory = pmm_alloc_blocks(num_blocks);
    return memory ? memory : NULL;  // Return NULL if allocation fails
}

int liballoc_free(void* ptr, int num_blocks)
{
    if (!ptr) return -1;  // Return error if the pointer is NULL

    // Use PMM to free the requested number of blocks
    pmm_free_blocks(ptr, num_blocks);
    return 0;  // Return 0 on success
}