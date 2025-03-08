#include "tss.h"
#include "gdt.h"
#include <stdarg.h>

void tss_init()
{
    // Get virtual address for TSS
    uint32_t tss_base = KERNEL_VMEM_START + ((uint32_t)&tss);
    
    // Set up TSS descriptor in GDT
    gdt_set_entry(5, tss_base, sizeof(TSS), 0x89, 0x40);
    
    // Load TSS
    tss.ss0 = 0x10;  // Kernel data segment
    tss.esp0 = KERNEL_VMEM_START + KERNEL_STACK_SIZE; // Virtual stack address
    
    // Load TSS selector
    asm volatile("ltr %%ax" : : "a"(0x28));  // 0x28 = 5th GDT entry
}
