#include "isr.h"
#include "idt.h"
#include "8259_pic.h"
#include "console.h"
#include "serial.h"

ISR g_interrupt_handlers[NO_INTERRUPT_HANDLERS];

char *exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available (No Math Coprocessor)",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection",
    "Page Fault",
    "Unknown Interrupt (intel reserved)",
    "x87 FPU Floating-Point Error (Math Fault)",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"};

void isr_register_interrupt_handler(size_t num, ISR handler)
{
    serial_printf("IRQ %zu registered\n", num);
    if (num < NO_INTERRUPT_HANDLERS)
        g_interrupt_handlers[num] = handler;
}

void isr_end_interrupt(size_t num)
{
    pic8259_eoi(num);
}

void isr_irq_handler(REGISTERS *reg)
{
    if (g_interrupt_handlers[reg->int_no] != NULL)
    {
        ISR handler = g_interrupt_handlers[reg->int_no];
        handler(reg);
    }
    pic8259_eoi(reg->int_no);
}

static void print_registers(REGISTERS *reg)
{
    serial_printf("REGISTERS:\n");
    serial_printf("err_code=%u\n", reg->err_code);
    serial_printf("eax=0x%x, ebx=0x%x, ecx=0x%x, edx=0x%x\n", reg->eax, reg->ebx, reg->ecx, reg->edx);
    serial_printf("edi=0x%x, esi=0x%x, ebp=0x%x, esp=0x%x\n", reg->edi, reg->esi, reg->ebp, reg->esp);
    serial_printf("eip=0x%x, cs=0x%x, ss=0x%x, eflags=0x%x, useresp=0x%x\n", reg->eip, reg->ss, reg->eflags, reg->useresp);
}

void isr_exception_handler(REGISTERS reg)
{
    if (reg.int_no < 32)
    {
        serial_printf("EXCEPTION: %s\n", exception_messages[reg.int_no]);
        print_registers(&reg);
        for (;;)
            __asm__ volatile("hlt");
    }
    if (g_interrupt_handlers[reg.int_no] != NULL)
    {
        ISR handler = g_interrupt_handlers[reg.int_no];
        handler(&reg);
    }
}
