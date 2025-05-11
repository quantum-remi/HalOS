#include "io.h"
#include "serial.h"

void fpu_set_control_word(const uint16_t cw)
{
    __asm__ volatile("fldcw %0" ::"m"(cw));
}

void fpu_enable()
{
    uint32_t eax, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=d"(edx) : "a"(1) : "ecx", "ebx");

    if (!(edx & (1 << 0)))
    {
        serial_printf("FPU not present\n");
        return;
    }

    uint32_t cr0, cr4;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); 
    cr0 |= (1 << 1); 
    __asm__ volatile("mov %0, %%cr0" ::"r"(cr0));

    if (edx & (1 << 25))
    {
        __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= (1 << 9);
        __asm__ volatile("mov %0, %%cr4" ::"r"(cr4));
        serial_printf("SSE enabled\n");
    }

    __asm__ volatile("fninit");
    fpu_set_control_word(0x037F);

    serial_printf("FPU initialized\n");
}