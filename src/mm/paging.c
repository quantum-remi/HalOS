#include "paging.h"

#include "console.h"
#include "io.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "kernel.h"

/*
References:-

https://wiki.osdev.org/Paging
https://wiki.osdev.org/Setting_Up_Paging


Page Directory:-

bit 0: present
bit 1: read_write
bit 2: user/supervisor
bit 3: write_through
bit 4: cache
bit 5: accessed
bit 6: dirty
bit 7: page_size
bit 8-11: available
bit 12-31: frame

Page Table:-

bit 0: present
bit 1: read_write
bit 2: user/supervisor
bit 3: write_through
bit 4: cache
bit 5: accessed
bit 6: dirty
bit 7: page_size
bit 8: global
bit 9-11: available
bit 12-31: frame

*/

BOOL g_is_paging_enabled = FALSE;
uint32 g_page_directory[1024] __attribute__((aligned(4096)));
uint32 g_page_tables[1024][1024] __attribute__((aligned(4096)));

#define PAGE_DIR_IDX(x) ((x) >> 22)
#define PAGE_TABLE_IDX(x) (((x) >> 12) & 0x3FF)

void map_page(uint32 phys_addr, uint32 virt_addr, uint32 flags)
{
    uint32 pd_idx = PAGE_DIR_IDX(virt_addr);
    uint32 pt_idx = PAGE_TABLE_IDX(virt_addr);

    g_page_tables[pd_idx][pt_idx] = (phys_addr & ~0xFFF) | (flags & 0xFFF);
    g_page_directory[pd_idx] = ((uint32)&g_page_tables[pd_idx]) | flags;
}

void page_fault_handler(REGISTERS* regs)
{
    uint32 faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));

    int present = !(regs->err_code & 0x1);
    int rw = regs->err_code & 0x2;
    int us = regs->err_code & 0x4;
    int reserved = regs->err_code & 0x8;

    serial_printf("Page fault at 0x%x ( %s %s %s %s)\n",
        faulting_address,
        present ? "present" : "not-present",
        rw ? "read-only" : "read-write",
        us ? "user-mode" : "supervisor-mode",
        reserved ? "reserved" : "non-reserved");

    panic("Page Fault");
}


void paging_init()
{
    uint32 i, cr0;
    uint32 phys_addr;
    
    serial_printf("paging_init(): Starting...\n");
    
    // Verify 4KB alignment
    if ((uint32)g_page_directory & 0xFFF) {
        panic("Page directory not aligned");
    }
    
    if ((uint32)g_page_tables & 0xFFF) {
        panic("Page tables not aligned");
    }

    // Clear structures
    memset(g_page_directory, 0, sizeof(g_page_directory));
    memset(g_page_tables, 0, sizeof(g_page_tables));

    // Identity map first 4MB
    for(i = 0; i < 1024; i++) {
        phys_addr = i * PAGE_SIZE;
        g_page_tables[0][i] = phys_addr | PAGE_PRESENT | PAGE_RW;
    }

    // Set first PD entry
    g_page_directory[0] = ((uint32)g_page_tables[0]) | PAGE_PRESENT | PAGE_RW;
    serial_printf("PD[0] = 0x%x\n", g_page_directory[0]);

    // Get current CR0
    // __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    // serial_printf("Current CR0: 0x%x\n", cr0);

    // Load CR3 and enable paging
    serial_printf("Loading page directory at 0x%x into CR3\n", (uint32)g_page_directory);
    
    __asm__ volatile(
        "cli\n"                      // Disable interrupts
        "movl %0, %%eax\n"          // Load PD address
        "movl %%eax, %%cr3\n"       // Set CR3
        "movl %%cr0, %%eax\n"       // Get CR0
        "orl $0x80000001, %%eax\n"  // Set PE and PG bits
        "movl %%eax, %%cr0\n"       // Update CR0
        :: "r"(g_page_directory)     // Input operand
        : "eax"                      // Clobber list
    );

    serial_printf("Paging enabled\n");

    g_is_paging_enabled = TRUE;
}

void* paging_get_physical_address(void* virtual_addr) 
{
    if (!g_is_paging_enabled)
        return virtual_addr;

    uint32 pdindex = PAGE_DIR_IDX((uint32)virtual_addr);
    uint32 ptindex = PAGE_TABLE_IDX((uint32)virtual_addr);
    uint32 offset = (uint32)virtual_addr & 0xFFF;

    if (!(g_page_directory[pdindex] & 1))
        return NULL; // Page directory entry not present

    uint32* pt = (uint32*)(g_page_directory[pdindex] & ~0xFFF);
    if (!(pt[ptindex] & 1))
        return NULL; // Page not present

    uint32 frame = pt[ptindex] & ~0xFFF;
    return (void*)(frame + offset);
}
// allocate page by calling pmm alloca block
void paging_allocate_page(void *virtual_addr)
{
    if (!g_is_paging_enabled)
        return;

    uint32 page_dir_index = (uint32)virtual_addr >> 22;
    uint32 page_table_index = ((uint32)virtual_addr >> 12) & 0x3FF;

    // Check page directory entry
    if (!(g_page_directory[page_dir_index] & PAGE_PRESENT))
    {
        uint32 pt_addr = (uint32)pmm_alloc_block();
        g_page_directory[page_dir_index] = pt_addr | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    // Check page table entry
    if (!(g_page_tables[page_dir_index][page_table_index] & PAGE_PRESENT))
    {
        uint32 page_addr = (uint32)pmm_alloc_block();
        g_page_tables[page_dir_index][page_table_index] = page_addr | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }
}

// clear the present, accessed & frame from page tables
void paging_free_page(void *virtual_addr)
{
    if (!g_is_paging_enabled)
    {
        return;
    }
    uint32 page_dir_index = (uint32)virtual_addr >> 22;
    uint32 page_table_index = (uint32)virtual_addr >> 12 & 0x03FF;

    if (!CHECK_BIT(g_page_directory[page_dir_index], 1))
    {
        serial_printf("free: page directory entry does not exists\n");
        return;
    }
    if (!CHECK_BIT(g_page_tables[page_table_index], 1))
    {
        serial_printf("free: page table entry does not exists\n");
        return;
    }
    // clear out from directory & table as we have allocated all the tables in paging_init()
    g_page_directory[page_dir_index] = 0;
}