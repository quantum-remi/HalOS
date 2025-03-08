#include "io.h"

uint8_t inportb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void outportb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %1, %0" ::"dN"(port), "a"(val));
}

uint16_t inportw(uint16_t port)
{
    uint16_t rv;
    __asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

void outportw(uint16_t port, uint16_t data)
{
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

uint16_t inports(uint16_t port)
{
    uint16_t rv;
    __asm__ volatile("inw %1, %0" : "=a"(rv) : "dN"(port));
    return rv;
}

void outports(uint16_t port, uint16_t data)
{
    __asm__ volatile("outw %1, %0" : : "dN"(port), "a"(data));
}

uint32_t inportl(uint16_t port)
{
    uint32_t rv;
    __asm__ volatile("inl %%dx, %%eax" : "=a"(rv) : "dN"(port));
    return rv;
}

void outportl(uint16_t port, uint32_t data)
{
    __asm__ volatile("outl %%eax, %%dx" : : "dN"(port), "a"(data));
}
