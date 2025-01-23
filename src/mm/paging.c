#include "paging.h"
#include "pmm.h"
#include "serial.h"
#include "kernel.h"
#include "string.h"
#include "isr.h"
#include "timer.h"
#define PAGE_TABLE_FLAGS 0x3 // Present and writable

extern uint32 __kernel_physical_start;
extern uint32 __kernel_virtual_start;

page_directory_t *current_directory = 0;


void page_fault_handler(REGISTERS *r) {
    uint32 faulting_address;
    __asm__ volatile("mov %%cr2, %0"
                 : "=r"(faulting_address));
    serial_printf("Segmentation fault 0x%x\n", faulting_address);
    while (1)
        ;
}

void paging_init()
{
    if (!current_directory)
    {
        serial_printf("Failed to allocate memory for page directory after multiple attempts\n");
        return;
    }
    memset(current_directory, 0, sizeof(page_directory_t));

    // Set up the page directory entries
    for (int i = 0; i < 1024; i++)
    {
        uint32 *page_table = (uint32 *)pmm_alloc_block();
        if (!page_table)
        {
            serial_printf("Failed to allocate memory for page table %d\n", i);
            return;
        }
        current_directory->tables[i] = (page_table_t *)((uint32)page_table | PAGE_TABLE_FLAGS); // Present and writable
        memset(page_table, 0, 4096);
        current_directory->tables[i] = (page_table_t *)((uint32)page_table | 3); // Present and writable
    }

    // Load the page directory
    load_page_directory(current_directory);
    isr_register_interrupt_handler(14, page_fault_handler);

    // Enable paging
    enable_paging();
}

void load_page_directory(page_directory_t *dir)
{
    if (!dir)
    {
        serial_printf("Error: Null page directory pointer\n");
        return;
    }
    serial_printf("Loading page directory at address 0x%x.\n", (uint32)dir);
    __asm__ volatile("mov %0, %%cr3" : : "r"(dir));
}

void enable_paging()
{
    // Enable paging extensions
    __asm__ volatile(
        "mov %cr4, %ebx \n"
        // Enable Page Size Extensions (PSE) to access 4MB pages
        "or $0x10, %ebx \n"
        "mov %ebx, %cr4 \n"

        // Enable paging
        "mov %cr0, %ebx \n"
        // Set the highest bit of CR0 to enable paging
        "or $0x80000000, %ebx \n"
        "mov %ebx, %cr0 \n"
    
        "mov %ebx, %cr0 \n"
    );
}


page_directory_t *get_page_directory()
{
    if (!current_directory)
    {
        serial_printf("Error: Current page directory is null\n");
    }
    return current_directory;
}