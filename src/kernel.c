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

#define BRAND_QEMU 1
#define BRAND_VBOX 2

KERNEL_MEMORY_MAP g_kmap;

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

void __cpuid(uint32 type, uint32 *eax, uint32 *ebx, uint32 *ecx, uint32 *edx)
{
    asm volatile("cpuid"
                 : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                 : "0"(type)); // put the type into eax
}

int cpuid_info(int print)
{
    char brand[49];
    uint32 eax, ebx, ecx, edx;
    uint32 type;

    memset(brand, 0, sizeof(brand));
    __cpuid(0x80000002, (uint32 *)brand + 0x0, (uint32 *)brand + 0x1, (uint32 *)brand + 0x2, (uint32 *)brand + 0x3);
    __cpuid(0x80000003, (uint32 *)brand + 0x4, (uint32 *)brand + 0x5, (uint32 *)brand + 0x6, (uint32 *)brand + 0x7);
    __cpuid(0x80000004, (uint32 *)brand + 0x8, (uint32 *)brand + 0x9, (uint32 *)brand + 0xa, (uint32 *)brand + 0xb);

    if (print)
    {
        printf("Brand: %s\n", brand);
        for (type = 0; type < 4; type++)
        {
            __cpuid(type, &eax, &ebx, &ecx, &edx);
            printf("type:0x%x, eax:0x%x, ebx:0x%x, ecx:0x%x, edx:0x%x\n", type, eax, ebx, ecx, edx);
        }
    }

    if (strstr(brand, "QEMU") != NULL)
        return BRAND_QEMU;

    return BRAND_VBOX;
}

BOOL is_echo(char *b)
{
    if ((b[0] == 'e') && (b[1] == 'c') && (b[2] == 'h') && (b[3] == 'o'))
        if (b[4] == ' ' || b[4] == '\0')
            return TRUE;
    return FALSE;
}

void shutdown()
{
    int brand = cpuid_info(0);
    // QEMU
    if (brand == BRAND_QEMU)
        outports(0x604, 0x2000);
    else
        // VBOX
        outports(0x4004, 0x3400);
}

void timer()
{
    int i;
    for (i = 0; i < 10; i++)
    {
        printf("Hello, World!\n");
        sleep(1);
    }
}

void memory()
{
    // display_kernel_memory_map(&g_kmap);
    printf("total_memory: %d KB, %d Bytes\n", g_kmap.system.total_memory, g_kmap.available.size);
    printf("start_addr: 0x%x, end_addr: 0x%x\n", g_kmap.available.start_addr, g_kmap.available.end_addr);
    printf("kstart_addr: 0x%x, kend_addr: 0x%x\n", g_kmap.kernel.k_start_addr, g_kmap.kernel.data_end_addr);
}

void ftoa(char *buf, float f)
{
    uint32 count = 1;
    const uint32 DEFAULT_DECIMAL_COUNT = 8;
    char int_part_buf[16];
    char *p;

    memset(int_part_buf, 0, sizeof(int_part_buf));
    // add integer part
    int x = (int)f;
    itoa(int_part_buf, 'd', x);
    p = int_part_buf;
    while (*p != '\0')
    {
        *buf++ = *p++;
    }
    *buf++ = '.';

    // example decimal = 3.14159 - 3 = 0.14159
    float decimal = f - x;
    if (decimal == 0)
        *buf++ = '0';
    else
    {
        while (decimal > 0)
        {
            uint32 y = decimal * 10; // y = 0.14159 * 10 = 1
            *buf++ = y + '0';
            decimal = (decimal * 10) - y; // decimal = (0.14159 * 10) - 1 = 0.4159
            count++;
            if (count == DEFAULT_DECIMAL_COUNT)
                break;
        }
    }
    *buf = '\0';
}

void float_print(const char *msg, float f, const char *end)
{
    char buf[32];
    memset(buf, 0, sizeof(buf));
    ftoa(buf, f);
    printf("%s%s%s", msg, buf, end);
}

void kmain(unsigned long magic, unsigned long addr)
{
    char buffer[255];
    MULTIBOOT_INFO *mboot_info;
    const char *shell = "$ ";

    gdt_init();
    idt_init();
    timer_init();

    console_init(COLOR_WHITE, COLOR_BLACK);
    keyboard_init();

    fpu_enable();

    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        mboot_info = (MULTIBOOT_INFO *)addr;
        memset(&g_kmap, 0, sizeof(KERNEL_MEMORY_MAP));
        if (get_kernel_memory_map(&g_kmap, mboot_info) < 0) {
            printf("error: failed to get kernel memory map\n");
            return;
        }
        // put the memory bitmap at the start of the available memory
        pmm_init(g_kmap.available.start_addr, g_kmap.available.size);
        // initialize atleast 1MB blocks of memory for our heap
        pmm_init_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 256);
        // initialize heap 256 blocks(1MB)
        void *start = pmm_alloc_blocks(256);
        void *end = start + (pmm_next_free_frame(1) * PMM_BLOCK_SIZE);
        kheap_init(start, end);


        while (1)
        {
            printf(shell);
            memset(buffer, 0, sizeof(buffer));
            getstr_bound(buffer, strlen(shell));
            if (strlen(buffer) == 0)
                continue;
            if (strcmp(buffer, "cpuid") == 0)
            {
                cpuid_info(1);
            }
            else if (strcmp(buffer, "help") == 0)
            {
                printf("Hal Terminal\n");
                printf("Commands: help, cpuid, echo, clear, memory, timer, shutdown\n");
            }
            else if (is_echo(buffer))
            {
                printf("%s\n", buffer + 5);
            }
            else if (strcmp(buffer, "shutdown") == 0)
            {
                shutdown();
            }
            else if (strcmp(buffer, "clear") == 0)
            {
                console_clear(COLOR_WHITE, COLOR_BLACK);
            }
            else if (strcmp(buffer, "timer") == 0)
            {
                timer();
            }
            else if (strcmp(buffer, "memory") == 0)
            {
                memory();
            }
            else if (strcmp(buffer, "snake") == 0)
            {
                snake_game();
            }
            else
            {
                printf("invalid command: %s\n", buffer);
            }
        }

        printf("Terminal started\n");
    }
}
        // int ret = vesa_init(800, 600, 32);
        // if (ret < 0) {
        //     printf("failed to init vesa graphics\n");
        //     goto done;
        // }
        // if (ret == 1) {
        //     // scroll to top
        //     for(int i = 0; i < MAXIMUM_PAGES; i++)
        //         console_scroll(SCROLL_UP);

        //     while (1) {
        //         // add scrolling to view all modes
        //         char c = kb_get_scancode();
        //         if (c == SCAN_CODE_KEY_UP)
        //             console_scroll(SCROLL_UP);
        //         if (c == SCAN_CODE_KEY_DOWN)
        //             console_scroll(SCROLL_DOWN);
        //     }
        // } else {
        //     // fill some colors
        //     uint32 x = 0;
        //     for (uint32 c = 0; c < 267; c++) {
        //         for (uint32 i = 0; i < 600; i++) {
        //             vbe_putpixel(x, i, VBE_RGB(c % 255, 0, 0));
        //         }
        //         x++;
        //     }
        //     for (uint32 c = 0; c < 267; c++) {
        //         for (uint32 i = 0; i < 600; i++) {
        //             vbe_putpixel(x, i, VBE_RGB(0, c % 255, 0));
        //         }
        //         x++;
        //     }
        //     for (uint32 c = 0; c < 267; c++) {
        //         for (uint32 i = 0; i < 600; i++) {
        //             vbe_putpixel(x, i, VBE_RGB(0, 0, c % 255));
        //         }
        //         x++;
        //     }
        // }

// done:
//         pmm_free_blocks(start, 256);
//         pmm_deinit_region(g_kmap.available.start_addr, PMM_BLOCK_SIZE * 256);
//     } else {
//         printf("error: invalid multiboot magic number\n");
//     }
// }

    //     while (1)
    //     {
    //         printf(shell);
    //         memset(buffer, 0, sizeof(buffer));
    //         getstr_bound(buffer, strlen(shell));
    //         if (strlen(buffer) == 0)
    //             continue;
    //         if (strcmp(buffer, "cpuid") == 0)
    //         {
    //             cpuid_info(1);
    //         }
    //         else if (strcmp(buffer, "help") == 0)
    //         {
    //             printf("Hal Terminal\n");
    //             printf("Commands: help, cpuid, echo, clear, memory, timer, shutdown\n");
    //         }
    //         else if (is_echo(buffer))
    //         {
    //             printf("%s\n", buffer + 5);
    //         }
    //         else if (strcmp(buffer, "shutdown") == 0)
    //         {
    //             shutdown();
    //         }
    //         else if (strcmp(buffer, "clear") == 0)
    //         {
    //             console_clear(COLOR_WHITE, COLOR_BLACK);
    //         }
    //         else if (strcmp(buffer, "timer") == 0)
    //         {
    //             timer();
    //         }
    //         else if (strcmp(buffer, "memory") == 0)
    //         {
    //             memory();
    //         }
    //         else if (strcmp(buffer, "snake") == 0)
    //         {
    //             // snake_game();
    //         }
    //         else
    //         {
    //             printf("invalid command: %s\n", buffer);
    //         }
    //     }

    //     printf("Terminal started\n");
    // }
