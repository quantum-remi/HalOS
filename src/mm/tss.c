#include "tss.h"
#include "gdt.h"
#include "string.h"
#include "console.h"

TSS g_tss;

extern uint32_t get_eip();

static void set_tss_entry(int index, uint16_t ss0, uint32_t esp0)
{
    uint32_t eip = get_eip();
    uint32_t base = (uint32_t)&g_tss;
    uint32_t limit = base + sizeof(g_tss);

    gdt_set_entry(index, base, limit, 0xE9, 0x00);
    memset(&g_tss, 0x0, sizeof(TSS));

    g_tss.ss0 = ss0;
    g_tss.esp0 = esp0;
    g_tss.eip = eip;
    g_tss.cs = 0x0b;
    g_tss.ss = 0x13;
    g_tss.ds = 0x13;
    g_tss.es = 0x13;
    g_tss.fs = 0x13;
    g_tss.gs = 0x13;
    g_tss.iomap_base = sizeof(TSS);
}

void tss_init(void)
{
    set_tss_entry(5, 0x10, 0x0100000);
    load_tss();
}

void tss_set_stack(uint32_t esp0)
{
    g_tss.esp0 = esp0;
}

void tss_print()
{
    console_printf("previous: 0x%x\n", g_tss.previous);
    console_printf("esp0: 0x%x, ss0: 0x%x\n", g_tss.esp0, g_tss.ss0);
    console_printf("esp1: 0x%x, ss1: 0x%x\n", g_tss.esp1, g_tss.ss1);
    console_printf("esp2: 0x%x, ss2: 0x%x\n", g_tss.esp2, g_tss.ss2);
    console_printf("cr3: 0x%x, eip: 0x%x, eflags: 0x%x\n", g_tss.cr3, g_tss.eip, g_tss.eflags);
    console_printf("eax: 0x%x, ecx: 0x%x, edx: 0x%x, ebx: 0x%x\n", g_tss.eax, g_tss.ecx, g_tss.edx, g_tss.ebx);
    console_printf("esp: 0x%x, ebp: 0x%x, esi: 0x%x, edi: 0x%x\n", g_tss.esp, g_tss.ebp, g_tss.esi, g_tss.edi);
    console_printf("es:0x%x, cs:0x%x, ss:0x%x, ds:0x%x, fs:0x%x, gs:0x%x\n", g_tss.es, g_tss.cs, g_tss.ss, g_tss.ds, g_tss.fs, g_tss.gs);
    console_printf("ldt: 0x%x, trap: 0x%x, iomap_base: 0x%x\n", g_tss.iomap_base, g_tss.trap, g_tss.iomap_base);
}
