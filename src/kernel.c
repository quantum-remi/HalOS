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

MULTIBOOT_INFO *g_mboot_ptr;

int get_kernel_memory_map(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info)
{
    uint32 i;

    if (kmap == NULL)
        return -1;
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

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP))
    {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        // make sure kernel is loaded at 0x100000 by bootloader(see linker.ld)
        if (mmap->addr_low == kmap->kernel.text_start_addr)
        {
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
    printf("kernel:\n");
    printf("  kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    printf("  text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    printf("  data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    printf("  rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    printf("  bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    printf("total_memory: %d KB\n", kmap->system.total_memory);
    printf("available:\n");
    printf("  start_adddr: 0x%x\n  end_addr: 0x%x\n  size: %d\n",
           kmap->available.start_addr, kmap->available.end_addr, kmap->available.size);
}


void kmain(unsigned long magic, unsigned long addr) {
    MULTIBOOT_INFO *mboot_info;

    g_mboot_ptr = (MULTIBOOT_INFO *)addr;

    // Initialize core subsystems
    gdt_init();
    idt_init();
    console_init(COLOR_WHITE, COLOR_BLACK);
    
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("Invalid multiboot magic number: 0x%x\n", magic);
        return;
    }

    // Get memory map first
    mboot_info = (MULTIBOOT_INFO *)addr;
    memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
    if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
        printf("Error: Failed to get kernel memory map\n");
        return;
    }

    printf("Memory info:\n");
    printf("Available memory: start=0x%x end=0x%x size=%d bytes\n",
           g_kmap.available.start_addr,
           g_kmap.available.end_addr,
           g_kmap.available.size);

    bios32_init();
    // Initialize PMM with proper addresses
    pmm_init(g_kmap.available.start_addr, g_kmap.available.size);
    if (pmm_get_max_blocks() == 0) {
        printf("Error: PMM initialization failed - no blocks available\n");
        printf("PMM info: start=0x%x size=%d\n", 
               g_kmap.available.start_addr,
               g_kmap.available.size);
        return;
    }

    // Initialize required memory regions
    pmm_init_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 256);
    
    // Initialize remaining subsystems
    timer_init();
    keyboard_init();
    fpu_enable();
    
    // Initialize heap
    void *heap_start = pmm_alloc_blocks(256);
    if (!heap_start) {
        printf("Error: Failed to allocate heap blocks\n");
        return;
    }
    void *heap_end = heap_start + (PMM_BLOCK_SIZE * 256);
    if (kheap_init(heap_start, heap_end) != 0) {
        printf("Error: Failed to initialize heap\n");
        return;
    }

    // printf("Initializing VBE...\n");
    // vbe_init();

    printf("System initialized successfully\n");
    shell();
}