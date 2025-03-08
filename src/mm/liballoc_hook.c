#include "liballoc_hook.h"
#include "vmm.h"
#include "serial.h"

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
    
    void *virt_addr = NULL;
    for (int i = 0; i < num_blocks; i++) {
        void *page = vmm_alloc_page();
        if (!page) {
            serial_printf("liballoc: Failed to allocate page %d of %d\n", i, num_blocks);
            // TODO: Free previously allocated pages
            return NULL;
        }
        if (i == 0) virt_addr = page;
    }
    
    serial_printf("liballoc: Allocated %d pages at virtual address 0x%x\n", 
                 num_blocks, (uint32_t)virt_addr);
    return virt_addr;
}

int liballoc_free(void *ptr, int num_blocks)
{
    if (!ptr)
        return -1;

    for (int i = 0; i < num_blocks; i++) {
        vmm_free_page((char*)ptr + (i * PAGE_SIZE));
    }
    return 0;
}
