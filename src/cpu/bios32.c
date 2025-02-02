#include <stdint.h>
#include <stddef.h>

#include "bios32.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "string.h"
#include "serial.h"

IDT_PTR g_real_mode_gdt;
IDT_PTR g_real_mode_idt;

extern GDT g_gdt[NO_GDT_DESCRIPTORS];

void (*exec_bios32_function)() = (void *)0x7c00;

/**
 init bios32 routine by setting 6 & 7 the entries
 this data will be copied when bios32_service() is called
 */
void bios32_init()
{
    // serial_printf("BIOS32: Initializing...\n");

    // Setup GDT entries for real mode transition
    gdt_set_entry(6, 0, 0xffffffff, 0x9A, 0x0f);
    gdt_set_entry(7, 0, 0xffffffff, 0x92, 0x0f);

    // Setup real mode environment
    g_real_mode_gdt.base_address = (uintptr_t)g_gdt;
    g_real_mode_gdt.limit = sizeof(g_gdt) - 1;
    g_real_mode_idt.base_address = 0;
    g_real_mode_idt.limit = 0x3ff;

    // serial_printf("BIOS32: GDT setup complete. Base=0x%x Limit=0x%x\n");
    // copy output registers to out
    g_real_mode_idt.base_address = 0;
    g_real_mode_idt.limit = 0x3ff;
}

/**
 copy data to assembly bios32_call.asm and execute code from 0x7c00 address
*/
void bios32_service(uint8_t interrupt, REGISTERS16 *in, REGISTERS16 *out)
{
    void *new_code_base = (void *)0x7c00;
    // serial_printf("BIOS32: Calling interrupt 0x%x\n", interrupt);
    // copy our GDT entries g_gdt to bios32_gdt_entries(bios32_call.asm)
    memcpy(&bios32_gdt_entries, g_gdt, sizeof(g_gdt));
    // set base_address of bios32_gdt_entries(bios32_call.asm) starting from 0x7c00
    g_real_mode_gdt.base_address = (uintptr_t)REBASE_ADDRESS((&bios32_gdt_entries));
    // copy g_real_mode_gdt to bios32_gdt_ptr(bios32_call.asm)
    memcpy(&bios32_gdt_ptr, &g_real_mode_gdt, sizeof(IDT_PTR));
    // copy g_real_mode_idt to bios32_idt_ptr(bios32_call.asm)
    memcpy(&bios32_idt_ptr, &g_real_mode_idt, sizeof(IDT_PTR));
    // copy all 16 bit in registers to bios32_in_reg16_ptr(bios32_call.asm)
    memcpy(&bios32_in_reg16_ptr, in, sizeof(REGISTERS16));
    // get out registers address defined in bios32_call.asm starting from 0x7c00
    void *in_reg16_address = REBASE_ADDRESS(&bios32_in_reg16_ptr);
    // copy bios interrupt number to bios32_int_number_ptr(bios32_call.asm)
    memcpy(&bios32_int_number_ptr, &interrupt, sizeof(uint8_t));
    // serial_printf("BIOS32: Copying data to 0x%x\n", new_code_base);
    // copy bios32_call.asm code to new_code_base address
    size_t size = (size_t)BIOS32_END - (size_t)BIOS32_START;
    memcpy(new_code_base, BIOS32_START, size);
    // execute the code from 0x7c00
    exec_bios32_function();
    // copy output registers to out
    in_reg16_address = REBASE_ADDRESS(&bios32_out_reg16_ptr);
    memcpy(out, in_reg16_address, sizeof(REGISTERS16));

    // serial_printf("BIOS32: Output registers: AX=0x%x\n", out->ax);
    // re-initialize the gdt and idt
    gdt_init();
    idt_init();
}
// bios interrupt call
// void int86(uint8 interrupt, REGISTERS16 *in, REGISTERS16 *out) {
//     bios32_service(interrupt, in, out);
// }
void int86(uint8_t interrupt, REGISTERS16 *in, REGISTERS16 *out)
{
    // console_printf("Debug: BIOS32 call int 0x%x\n", interrupt);
    // console_printf("Debug: Input registers: AX=0x%x ES=0x%x DI=0x%x\n",
    //        in->ax, in->es, in->di);

    bios32_service(interrupt, in, out);

    // console_printf("Debug: Output registers: AX=0x%x\n", out->ax);
}
