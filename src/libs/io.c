#include "io.h"

uint8 inportb(uint16 port)
{
    uint8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outportb(uint16 port, uint8 val)
{
    __asm__ volatile("outb %1, %0" ::"dN"(port), "a"(val));
}

uint16 inports(uint16 port)
{
    uint16 rv;
    __asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

void outports(uint16 port, uint16 data)
{
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

uint32 inportl(uint16 port)
{
    uint32 rv;
    __asm__ volatile("inl %%dx, %%eax" : "=a"(rv) : "dN"(port));
    return rv;
}

void outportl(uint16 port, uint32 data)
{
    __asm__ volatile("outl %%eax, %%dx" : : "dN"(port), "a"(data));
}
