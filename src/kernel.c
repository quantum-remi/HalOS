#include "kernel.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "multiboot.h"
#include "timer.h"
#include "paging.h"
#include "string.h"
#include "pmm.h"
#include "fpu.h"
#include "vesa.h"
#include "shell.h"
#include "bios32.h"
#include "serial.h"
#include "tss.h"
// #include "liballoc.h"
MULTIBOOT_INFO *g_mboot_ptr;

int get_kernel_memory_map(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info) {
    uint32 i;

    if (kmap == NULL) return -1;
    kmap->kernel.k_start_addr = (uint32)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32)&__kernel_section_end - (uint32)&__kernel_section_start);

    kmap->kernel.text_start_addr = (uint32)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32)&__kernel_text_section_end - (uint32)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32)&__kernel_data_section_end;
    kmap->kernel.data_len = ((uint32)&__kernel_data_section_end - (uint32)&__kernel_data_section_start);

    kmap->kernel.rodata_start_addr = (uint32)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len = ((uint32)&__kernel_rodata_section_end - (uint32)&__kernel_rodata_section_start);

    kmap->kernel.bss_start_addr = (uint32)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32)&__kernel_bss_section_end;
    kmap->kernel.bss_len = ((uint32)&__kernel_bss_section_end - (uint32)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_low + mboot_info->mem_high;

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP)) {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
        // make sure kernel is loaded at 0x100000 by bootloader(see linker.ld)
        if (mmap->addr_low == kmap->kernel.text_start_addr) {
            // set available memory starting from end of our kernel, leaving 1MB size for functions exceution
            kmap->available.start_addr = kmap->kernel.k_end_addr + 1024 * 1024;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;
            // get availabel memory in bytes
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
    }

    return -1;
}
void display_kernel_memory_map(KERNEL_MEMORY_MAP *kmap)
{
    console_printf("kernel:\n");
    console_printf("  kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    console_printf("  text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    console_printf("  data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    console_printf("  rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    console_printf("  bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    console_printf("total_memory: %d KB\n", kmap->system.total_memory);
    console_printf("available:\n");
    console_printf("  start_adddr: 0x%x\n  end_addr: 0x%x\n  size: %d\n",
           kmap->available.start_addr, kmap->available.end_addr, kmap->available.size);
}



void kmain(unsigned long magic, unsigned long addr) {
    MULTIBOOT_INFO *mboot_info;
    g_mboot_ptr = (MULTIBOOT_INFO *)addr;
    // Initialize serial port
    serial_init();
    serial_printf("\n=== Boot Sequence Started ===\n");
    // Initialize core subsystems
    struct resolution resolution;
    serial_printf("Initializing GDT...\n");
    gdt_init();
    serial_printf("Initializing IDT...\n");
    idt_init();
    serial_printf("Initializing TSS...\n");
    tss_init();
    
    resolution.x = 1024;
    resolution.y = 768;
    serial_printf("setting up resolution to %d x %d\n", resolution.x, resolution.y);
    serial_printf("Initializing console...\n");
    if(vesa_init(resolution.x, resolution.y, 32) != 0) {
        serial_printf("ERROR: VESA init failed\n");
        return;
    }
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_printf("ERROR: Invalid multiboot magic number!\n");
        console_printf("Invalid multiboot magic number: 0x%x\n", magic);
        return;
    }

    // Get memory map first
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        mboot_info = (MULTIBOOT_INFO *)addr;
        memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
        if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
            serial_printf("error: failed to get kernel memory map\n");
            return;
        }

        pmm_init(g_kmap.available.start_addr, g_kmap.available.size);

        serial_printf("Max blocks: %d\n", pmm_get_max_blocks());
        // initialize a region of memory of size (4096 * 10), 10 blocks memory
        pmm_init_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 10);
        serial_printf("PMM init\n");

        serial_printf("Initializing paging...\n");
        paging_init();

        paging_allocate_page((void *)0x8973456);
        serial_printf("physical address: 0x%x\n", paging_get_physical_address((void *)0x8973456));

        int *x = (int *)paging_get_physical_address((void *)0x8973456);
        x[0] = 123;

        paging_free_page((void *)0x8973456);


        pmm_deinit_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 10);
        serial_printf("PMM Deinit\n");

        serial_printf("Initializing BIOS32...\n");
        bios32_init();
        
        // Initialize remaining subsystems
        serial_printf("Initializing timer...\n");    
        timer_init();
        serial_printf("Initializing keyboard...\n");
        keyboard_init();
        serial_printf("Initializing FPU...\n");
        fpu_enable();
        
        serial_printf("VESA initialized\n");
        
        console_init(COLOR_WHITE, COLOR_BLACK);
        serial_printf("Console initialized\n");

        serial_printf("System initialized successfully\n");
        console_printf("System initialized successfully\n");

        shell();
    }
    else {
        serial_printf("ERROR: Invalid multiboot magic number!\n");
        console_printf("Invalid multiboot magic number: 0x%x\n", magic);
    }
}