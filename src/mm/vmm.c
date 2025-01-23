#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "kernel.h"

extern uint32 __kernel_physical_start;
extern uint32 __kernel_virtual_start;

void vmm_init() {
    paging_init();
}

void vmm_map_page(void *phys, void *virt) {
    uint32 pd_index = (uint32)virt >> 22;
    uint32 pt_index = (uint32)virt >> 12 & 0x03FF;

    page_directory_t *pd = get_page_directory();
    if (!pd->tables[pd_index]) {
        pd->tables[pd_index] = (page_table_t *)pmm_alloc_block();
        if (!pd->tables[pd_index]) {
            serial_printf("Failed to allocate memory for page table\n");
            return;
        }
        memset(pd->tables[pd_index], 0, sizeof(page_table_t));
        serial_printf("Page table allocated and cleared at address 0x%x.\n", (uint32)pd->tables[pd_index]);
    }

    page_table_t *pt = pd->tables[pd_index];
    pt->pages[pt_index].present = 1;
    pt->pages[pt_index].rw = 1;
    pt->pages[pt_index].frame = ((uint32)phys - __kernel_physical_start) >> 12;
    serial_printf("Mapped physical address 0x%x to virtual address 0x%x.\n", (uint32)phys, (uint32)virt);
}

void *vmm_alloc_blocks(int num_blocks) {
    void *phys_addr = pmm_alloc_block();
    if (!phys_addr) {
        serial_printf("Failed to allocate physical memory block\n");
        return NULL;
    }

    for (int i = 0; i < num_blocks; i++) {
        void *virt_addr = (void *)((uint32)phys_addr + i * PMM_BLOCK_SIZE + __kernel_virtual_start);
        vmm_map_page(phys_addr, virt_addr);
    }
    return phys_addr;
}

void vmm_free_blocks(void *ptr, int num_blocks) {
    for (int i = 0; i < num_blocks; i++) {
        void *virt_addr = (void *)((uint32)ptr + i * PMM_BLOCK_SIZE + __kernel_virtual_start);
        uint32 pd_index = (uint32)virt_addr >> 22;
        uint32 pt_index = (uint32)virt_addr >> 12 & 0x03FF;

        page_directory_t *pd = get_page_directory();
        page_table_t *pt = pd->tables[pd_index];
        if (pt) {
            pt->pages[pt_index].present = 0;
            serial_printf("Unmapped virtual address 0x%x.\n", (uint32)virt_addr);
        }
    }
    pmm_free_block(ptr);
}