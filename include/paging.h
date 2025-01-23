#ifndef PAGING_H
#define PAGING_H

#include "types.h"
#include "isr.h"

#define PAGE_SIZE 4096
#define PAGE_DIR_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024

// Bit manipulation macros - fix pointer dereferencing
#define CHECK_BIT(var,pos) ((*(uint32*)&(var)) & (1U<<(pos)))
#define SET_BIT(var,pos) (*(uint32*)&(var) |= (1U<<(pos)))
#define CLEAR_BIT(var,pos) (*(uint32*)&(var) &= ~(1U<<(pos)))

// Page flags
#define PAGE_PRESENT 1
#define PAGE_RW 2
#define PAGE_USER 4
#define PAGE_ACCESSED 32
#define PAGE_DIRTY 64

extern uint32 g_page_directory[PAGE_DIR_ENTRIES] __attribute__((aligned(4096)));
extern uint32 g_page_tables[PAGE_DIR_ENTRIES][PAGE_TABLE_ENTRIES] __attribute__((aligned(4096)));
extern BOOL g_is_paging_enabled;

void paging_init(void);
void page_fault_handler(REGISTERS* regs);
void paging_allocate_page(void *virtual_addr);
void paging_free_page(void *virtual_addr);
void map_page(uint32 phys_addr, uint32 virt_addr, uint32 flags);
void* paging_get_physical_address(void* virtual_addr);

#endif