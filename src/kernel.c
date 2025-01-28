#include <stdint.h>
#include <stddef.h>

#include "bios32.h"
#include "console.h"
#include "fpu.h"
#include "gdt.h"
#include "ide.h"
#include "idt.h"
#include "kernel.h"
#include "keyboard.h"
#include "liballoc.h"
#include "multiboot.h"
#include "pmm.h"
#include "pci.h"
#include "paging.h"
#include "serial.h"
#include "shell.h"
#include "string.h"
#include "timer.h"
#include "tss.h"
#include "vesa.h"
// #include "vmm.h"


int get_kernel_memory_map(KERNEL_MEMORY_MAP *kmap, multiboot_info_t *mboot_info)
{
    if (kmap == NULL)
        return -1;

    kmap->kernel.k_start_addr = (uint32_t)&__kernel_physical_start;
    kmap->kernel.k_end_addr = (uint32_t)&__kernel_physical_end;
    kmap->kernel.k_len = ((uint32_t)&__kernel_physical_end - (uint32_t)&__kernel_physical_start);

    kmap->kernel.text_start_addr = (uint32_t)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32_t)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32_t)&__kernel_text_section_end - (uint32_t)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32_t)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32_t)&__kernel_data_section_end;
    kmap->kernel.data_len = ((uint32_t)&__kernel_data_section_end - (uint32_t)&__kernel_data_section_start);

    kmap->kernel.bss_start_addr = (uint32_t)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32_t)&__kernel_bss_section_end;
    kmap->kernel.bss_len = ((uint32_t)&__kernel_bss_section_end - (uint32_t)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_lower + mboot_info->mem_upper;
    return 0;
}

void display_kernel_memory_map(KERNEL_MEMORY_MAP *kmap)
{
    console_printf("kernel:\n");
    console_printf("  kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %zu bytes\n",
                   kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    console_printf("  text-start: 0x%x, text-end: 0x%x, TOTAL: %zu bytes\n",
                   kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    console_printf("  data-start: 0x%x, data-end: 0x%x, TOTAL: %zu bytes\n",
                   kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    console_printf("  bss-start: 0x%x, bss-end: 0x%x, TOTAL: %zu bytes\n",
                   kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);
}

void panic(char *msg)
{
    serial_printf("PANIC: %s\n", msg);
    for (;;)
        __asm__ volatile("hlt");
}

void kmain(unsigned long magic, unsigned long addr)
{
    // Initialize serial port
    serial_init();
    serial_printf("\n=== Boot Sequence Started ===\n");

    // Initialize core subsystems with detailed logging
    struct resolution resolution = { .x = 1024, .y = 768 };

    serial_printf("Initializing GDT...\n");
    gdt_init();

    serial_printf("Initializing IDT...\n");
    idt_init();

    serial_printf("Initializing TSS...\n");
    tss_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid multiboot magic number");
        return;
    }

    multiboot_info_t *mboot_info = (multiboot_info_t *)addr;

    // Initialize kernel memory map
    if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
        panic("Failed to get kernel memory map");
        return;
    }

    // Initialize physical memory manager
    pmm_init(g_kmap.system.total_memory * 1024, (uint32_t *)mboot_info->mmap_addr);

    // // Initialize BIOS32
    // serial_printf("Initializing BIOS32...\n");
    // bios32_init();

    // Set up graphics resolution
    if (vesa_init(resolution.x, resolution.y, 32) != 0) {
        serial_printf("ERROR: Resolution init failed\n");
        return;
    }
    serial_printf("Resolution set to %d x %d\n", resolution.x, resolution.y);

    // Initialize remaining subsystems
    serial_printf("Initializing timer...\n");
    timer_init();

    serial_printf("Initializing keyboard...\n");
    keyboard_init();

    serial_printf("Enabling FPU...\n");
    fpu_enable();

    serial_printf("Initializing ATA...\n");
    ata_init();

    serial_printf("Initializing PCI...\n");
    pci_init();

    // Initialize console
    console_init(VESA_COLOR_WHITE, VESA_COLOR_BLACK);
    serial_printf("Console initialized\n");

    serial_printf("System initialized successfully\n");
    console_printf("System initialized successfully\n");

    // Start shell
    shell();
}
