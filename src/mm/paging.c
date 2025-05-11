#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "vmm.h"
#include <stdbool.h>
#include "isr.h"
#include "8259_pic.h"

extern uint32_t __kernel_physical_start;
extern uint32_t __kernel_physical_end;

static uint32_t *page_directory __attribute__((aligned(4096))) = NULL;
static bool paging_active = false; // Track if paging is enabled

void paging_init()
{
    // Allocate page directory (must be 4KB aligned)
    page_directory = pmm_alloc_block();
    if (!page_directory)
    {
        serial_printf("Paging: Failed to allocate page directory!\n");
        serial_printf("Paging: Failed to allocate first page table!\n");
        return;
    }
    // serial_printf("Paging: Page directory allocated at 0x%x\n", (uint32_t)page_directory);

    memset(page_directory, 0, PAGE_SIZE);

    uint32_t *first_page_table = pmm_alloc_block();
    if (!first_page_table)
        return;

    memset(first_page_table, 0, PAGE_SIZE);

    for (uint32_t i = 0; i < 2048; i++)
    {
        first_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    page_directory[0] = ((uint32_t)first_page_table & 0xFFFFF000) | PAGE_PRESENT | PAGE_WRITABLE;

    page_directory[768] = ((uint32_t)first_page_table) | PAGE_PRESENT | PAGE_WRITABLE;

    // serial_printf("Paging: Page directory initialized at 0x%x\n", (uint32_t)page_directory);
}

static inline void load_page_directory(uint32_t pd_addr)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(pd_addr) : "memory");
}

static inline void enable_paging_internal()
{
    __asm__ volatile(
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        "jmp 1f\n"
        "1:\n" ::: "eax", "memory");
}

void paging_enable(uint32_t page_directory_addr)
{
    if (!page_directory_addr)
    {
        serial_printf("Paging: Invalid page directory address\n");
        return;
    }

    // serial_printf("Paging: Loading page directory at 0x%X\n", page_directory_addr);
    load_page_directory(page_directory_addr);

    // serial_printf("Paging: Enabling paging\n");
    enable_paging_internal();

    __asm__ volatile(
        "pushl $0x08\n" // Code segment selector
        "pushl $1f\n"   // Return address
        "retf\n"        // Far return
        "1:\n");

    paging_active = true;
    page_directory = (uint32_t*)(0xC0000000 + (uint32_t)page_directory);
    serial_printf("Paging: Enabled successfully\n");
}

bool paging_map_page(uint32_t phys_addr, uint32_t virt_addr, uint32_t flags)
{
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PAGE_PRESENT))
    {
        uint32_t *new_table = pmm_alloc_block();
        if (!new_table)
        {
            serial_printf("Paging: Failed to allocate page table for PDE %d\n", pd_index);
            return false;
        }
        memset(new_table, 0, PAGE_SIZE);

        uint32_t pde_flags = PAGE_PRESENT | PAGE_WRITABLE;
        page_directory[pd_index] = (uint32_t)new_table | pde_flags;
    }

    uint32_t *page_table = (uint32_t *)(page_directory[pd_index] & ~0xFFF);
    if (paging_active)
    {
        page_table = (uint32_t *)(0xC0000000 + (uint32_t)page_table);
    }

    page_table[pt_index] = (phys_addr & ~0xFFF) | flags | PAGE_PRESENT;
    return true;
}

void paging_unmap_page(uint32_t virt_addr)
{
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PAGE_PRESENT))
    {
        serial_printf("Paging: Page directory entry not present for virtual address 0x%x\n", virt_addr);
        return;
    }

    uint32_t *pt = (uint32_t *)(page_directory[pd_index] & ~0xFFF);

    if (!(pt[pt_index] & PAGE_PRESENT))
    {
        // serial_printf("Paging: Page table entry not present for virtual address 0x%x\n", virt_addr);
        return;
    }

    pt[pt_index] = 0; 

    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

uint32_t *get_page_directory()
{
    // serial_printf("Paging: get_page_directory returning 0x%x\n", (uint32_t)page_directory);
    return page_directory;
}