#include "isr.h"
#include "idt.h"
#include "io.h"
#include "8259_pic.h"

void pic8259_init()
{
    outportb(PIC1_COMMAND, ICW1);
    outportb(PIC2_COMMAND, ICW1);

    outportb(PIC1_DATA, 0x20);
    outportb(PIC2_DATA, 0x28);

    outportb(PIC1_DATA, 0x04); 
    outportb(PIC2_DATA, 0x02);

    outportb(PIC1_DATA, ICW4_8086);
    outportb(PIC2_DATA, ICW4_8086);

    outportb(PIC1_DATA, 0xFF);
    outportb(PIC2_DATA, 0xFF);
}

void pic8259_eoi(uint8_t irq) {
    if (irq >= 8) {
        outportb(PIC2_COMMAND, PIC_EOI);
        outportb(PIC1_COMMAND, PIC_EOI); 
    } else {
        outportb(PIC1_COMMAND, PIC_EOI); 
    }
}

int pic8259_is_spurious(uint8_t irq)
{
    if (irq == 7)
        return (inportb(PIC1_COMMAND) & 0x80) == 0;
    if (irq == 15)
        return (inportb(PIC2_COMMAND) & 0x80) == 0;
    return 1;
}

void pic8259_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inportb(port) | (1 << irq);
    outportb(port, value);
}

void pic8259_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t mask;

    if (irq >= 8)
    {
        port = PIC2_DATA; // Slave PIC data port: 0xA1
        irq -= 8;
    }
    else
    {
        port = PIC1_DATA; // Master PIC data port: 0x21
    }

    mask = inportb(port) & ~(1 << irq);
    outportb(port, mask);
}

uint16_t pic8259_get_mask()
{
    uint8_t mask1 = inportb(PIC1_DATA);
    uint8_t mask2 = inportb(PIC2_DATA);
    return (mask2 << 8) | mask1;
}