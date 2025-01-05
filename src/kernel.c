#include "kernel.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "io.h"
#include "multiboot.h"
#include "timer.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "snake.h"
#include "fpu.h"
#include "vesa.h"
#include "shell.h"
#include "bios32.h"
#include "serial.h"

MULTIBOOT_INFO *g_mboot_ptr;

int get_kernel_memory_map(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info)
{
    uint32 i;
    serial_printf("Initializing kernel memory map...\n");

    if (kmap == NULL)
        serial_printf("ERROR: kmap is NULL!\n");
        return -1;
    kmap->kernel.k_start_addr = (uint32)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32)&__kernel_section_end - (uint32)&__kernel_section_start);

    serial_printf("kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);

    kmap->kernel.text_start_addr = (uint32)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32)&__kernel_text_section_end - (uint32)&__kernel_text_section_start);

    serial_printf("text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);

    kmap->kernel.data_start_addr = (uint32)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32)&__kernel_data_section_end;
    kmap->kernel.data_len = ((uint32)&__kernel_data_section_end - (uint32)&__kernel_data_section_start);

    serial_printf("data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);

    kmap->kernel.rodata_start_addr = (uint32)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len = ((uint32)&__kernel_rodata_section_end - (uint32)&__kernel_rodata_section_start);

    serial_printf("rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);

    kmap->kernel.bss_start_addr = (uint32)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32)&__kernel_bss_section_end;
    kmap->kernel.bss_len = ((uint32)&__kernel_bss_section_end - (uint32)&__kernel_bss_section_start);

    serial_printf("bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    kmap->system.total_memory = mboot_info->mem_low + mboot_info->mem_high;

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP))
    {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        serial_printf("Memory region: start=0x%x, len=%d\n", mmap->addr_low, mmap->len_low);
        // make sure kernel is loaded at 0x100000 by bootloader(see linker.ld)
        if (mmap->addr_low == kmap->kernel.text_start_addr)
        {
            // set available memory starting from end of our kernel, leaving 1MB size for functions exceution
            kmap->available.start_addr = kmap->kernel.k_end_addr + 1024 * 1024;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;

            serial_printf("Available memory: start=0x%x, len=%d\n", kmap->available.start_addr, kmap->available.end_addr - kmap->available.start_addr);
            // get availabel memory in bytes
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
    }
    serial_printf("ERROR: Failed to find kernel memory region!\n");
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
    serial_printf("Initializing GDT...\n");
    gdt_init();
    serial_printf("Initializing IDT...\n");
    idt_init();
    serial_printf("Initializing console...\n");
    if(vesa_init(800, 600, 32) != 0) {
        serial_printf("ERROR: VESA init failed\n");
        return;
    }
    serial_printf("VESA initialized\n");
    
    console_init(COLOR_WHITE, COLOR_BLACK);
    serial_printf("Console initialized\n");
    
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_printf("ERROR: Invalid multiboot magic number!\n");
        console_printf("Invalid multiboot magic number: 0x%x\n", magic);
        return;
    }
    
    REGISTERS16 in = {0}, out = {0};
    in.ax = 0xE820;
    in.cx = 0x14;
    in.dx = 0x534D4150; // "SMAP"
    in.es = 0x0;
    in.di = 0x0;

    // Try using a different function code
    in.ax = 0xE801;

    serial_printf("Calling int 0x15...\n");
    int86(0x15, &in, &out);

     // Print out the return values
    serial_printf("AX: 0x%x\n", out.ax);
    serial_printf("BX: 0x%x\n", out.bx);
    serial_printf("CX: 0x%x\n", out.cx);
    serial_printf("DX: 0x%x\n", out.dx);

    // Check if the BIOS returned an error code
    if (out.ax != 0x534D4150) { // "SMAP"
        serial_printf("Error: BIOS returned error code 0x%x\n", out.ax);
        // Handle the error here
    }

    // Parse the memory map data
    uint32 base_address = out.bx;
    uint32 size = out.cx;
    uint32 type = out.dx;

    serial_printf("Memory region: base=0x%x size=0x%x type=0x%x\n", base_address, size, type);


    // Get memory map first
    mboot_info = (MULTIBOOT_INFO *)addr;
    memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
    if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
        console_printf("Error: Failed to get kernel memory map\n");
        serial_printf("Error: Failed to get kernel memory map\n");
        console_printf("System initialization failed\n");
        serial_printf("Memory info:\n");
        serial_printf("Available memory: start=0x%x end=0x%x size=%d bytes\n",
            g_kmap.available.start_addr,
            g_kmap.available.end_addr,
            g_kmap.available.size);
        serial_printf("MBoot info:\n");
        serial_printf("  flags: 0x%x\n", mboot_info->flags);
        serial_printf("  mem_lower: 0x%x\n", mboot_info->mem_low);
        serial_printf("  mem_upper: 0x%x\n", mboot_info->mem_high);
        serial_printf("  boot_device: 0x%x\n", mboot_info->boot_device);
        serial_printf("  cmdline: %s\n", mboot_info->cmdline);
        return;
    }
    serial_printf("Kernel memory map initialized\n");

    serial_printf("Initializing BIOS32...\n");
    bios32_init();
    // Initialize PMM with proper addresses
    pmm_init(g_kmap.available.start_addr, g_kmap.available.size);
    if (pmm_get_max_blocks() == 0) {
        console_printf("Error: PMM initialization failed - no blocks available\n");
        console_printf("PMM info: start=0x%x size=%d\n", 
               g_kmap.available.start_addr,
               g_kmap.available.size);
        return;
    }
    serial_printf("PMM initialized successfully\n");
    // Initialize required memory regions
    serial_printf("Initializing memory regions...\n");
    pmm_init_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 256);
    serial_printf("Memory regions initialized\n");

    // Initialize remaining subsystems
    serial_printf("Initializing timer...\n");    
    timer_init();
    serial_printf("Initializing keyboard...\n");
    keyboard_init();
    serial_printf("Initializing FPU...\n");
    fpu_enable();
    
    serial_printf("Initializing paging...\n");
    // Initialize heap
    void *heap_start = pmm_alloc_blocks(256);
    if (!heap_start) {
        console_printf("Error: Failed to allocate heap blocks\n");
        return;
    }
    void *heap_end = heap_start + (PMM_BLOCK_SIZE * 256);
    if (kheap_init(heap_start, heap_end) != 0) {
        console_printf("Error: Failed to initialize heap\n");
        return;
    }

    // printf("Initializing VBE...\n");
    // vbe_init();
    serial_printf("System initialized successfully\n");
    console_printf("System initialized successfully\n");
    shell();
}