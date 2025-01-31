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

    serial_printf("Initializing GDT...\n");
    gdt_init();

    serial_printf("Initializing IDT...\n");
    idt_init();

    serial_printf("Initializing TSS...\n");
    tss_init();

    multiboot_info_t *mboot_info = (multiboot_info_t *)addr;
    
    if (mboot_info->flags & MULTIBOOT_INFO_VBE_INFO) {
        serial_printf("VBE control info: 0x%x\n", mboot_info->vbe_control_info);
        serial_printf("VBE mode info: 0x%x\n", mboot_info->vbe_mode_info);
    }

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Invalid multiboot magic number");
        return;
    }


    // Initialize kernel memory map
    if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
        panic("Failed to get kernel memory map");
        return;
    }

    // Initialize physical memory manager
    pmm_init(g_kmap.system.total_memory * 1024, (uint32_t *)mboot_info->mmap_addr);

    // // Initialize BIOS32
    serial_printf("Initializing BIOS32...\n");
    bios32_init();

    // Set up graphics resolution
        // Initialize VESA graphics
    if(vesa_init(mboot_info) != 0) {
        serial_printf("Failed to initialize VESA graphics\n");
        // Fall back to text mode or safe default
        console_printf("Graphics initialization failed!\n");
    } else {
        // Graphics initialized successfully
        console_printf("VESA %ux%ux%u initialized\n", 
                      vbe_get_width(), vbe_get_height(), vbe_get_bpp());

        // Draw test pattern
        for(int y = 0; y < 100; y++) {
            for(int x = 0; x < 100; x++) {
                vbe_putpixel(x, y, vbe_rgb(x*2, y*2, 0));
            }
        }
    }


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
