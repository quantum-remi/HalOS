#include "tss.h"
#include "gdt.h"
#include "string.h"
#include "serial.h"

// Aligned TSS instance with kernel stack
__attribute__((aligned(4096))) static TSS tss;

void tss_init()
{
    // Initialize TSS structure
    memset(&tss, 0, sizeof(TSS));
    tss.ss0 = 0x10; // Kernel data segment
    tss.esp0 = 0xC0000000 + 0x9000; // Example virtual stack address

    // Add TSS descriptor to GDT at index 5
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(TSS) - 1;

    // Flags:
    // - Present = 1 (0x80)
    // - DPL = 0 (0x00)
    // - Type = 32-bit Available TSS (0x9)
    gdt_set_entry(5, base, limit, 0x89, 0x00);


    // Load TSS into task register (0x28 = 5th entry * 8)
    __asm__ volatile("ltr %%ax" : : "a"(0x28));
    serial_printf("TSS initialized at 0x%x\n", base);
}

void tss_set_stack(uint32_t esp0)
{
    tss.esp0 = esp0;
    serial_printf("TSS kernel stack updated to 0x%x\n", esp0);
}