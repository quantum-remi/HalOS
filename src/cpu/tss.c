#include "tss.h"
#include "gdt.h"
#include "string.h"
#include "serial.h"

__attribute__((aligned(4096))) static TSS tss;

void tss_init()
{
    memset(&tss, 0, sizeof(TSS));
    tss.ss0 = 0x10; 
    tss.esp0 = 0xC0000000 + 0x9000; 
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(TSS) - 1;

    gdt_set_entry(5, base, limit, 0x89, 0x00);


    __asm__ volatile("ltr %%ax" : : "a"(0x28));
    serial_printf("TSS initialized at 0x%x\n", base);
}

void tss_set_stack(uint32_t esp0)
{
    tss.esp0 = esp0;
    serial_printf("TSS kernel stack updated to 0x%x\n", esp0);
}