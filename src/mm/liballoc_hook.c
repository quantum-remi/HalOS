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
    // // Use VMM to allocate the requested number of blocks
    // void *memory = vmm_alloc_blocks(num_blocks);
    // return memory ? memory : NULL; // Return NULL if allocation fails
}

int liballoc_free(void *ptr, int num_blocks)
{
    // if (!ptr)
    //     return -1; // Return error if the pointer is NULL

    // // Use VMM to free the requested number of blocks
    // vmm_free_blocks(ptr, num_blocks);
    // return 0; // Return 0 on success
}