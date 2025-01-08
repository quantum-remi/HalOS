#include "shell.h"
#include "console.h"
#include "string.h"
#include <kernel.h>
#include "io.h"
#include "timer.h"
#include "snake.h"
#include "vesa.h"
#include "keyboard.h"
#include "tss.h"

#define BRAND_QEMU 1
#define BRAND_VBOX 2

KERNEL_MEMORY_MAP g_kmap;

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
        console_printf("Brand: %s\n", brand);
        for (type = 0; type < 4; type++)
        {
            __cpuid(type, &eax, &ebx, &ecx, &edx);
            console_printf("type:0x%x, eax:0x%x, ebx:0x%x, ecx:0x%x, edx:0x%x\n", type, eax, ebx, ecx, edx);
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
    uptime();
    sleep(1);

}

void memory()
{
    // display_kernel_memory_map(&g_kmap);
    console_printf("total_memory: %d MB, %d Bytes\n", g_kmap.system.total_memory / 1024, g_kmap.available.size);
    console_printf("start_addr: 0x%x, end_addr: 0x%x\n", g_kmap.available.start_addr, g_kmap.available.end_addr);
    console_printf("kstart_addr: 0x%x, kend_addr: 0x%x\n", g_kmap.kernel.k_start_addr, g_kmap.kernel.data_end_addr);
    console_printf("text_start_addr: 0x%x, text_end_addr: 0x%x\n", g_kmap.kernel.text_start_addr, g_kmap.kernel.text_end_addr);
    console_printf("data_start_addr: 0x%x, data_end_addr: 0x%x\n", g_kmap.kernel.data_start_addr, g_kmap.kernel.data_end_addr);
    console_printf("rodata_start_addr: 0x%x, rodata_end_addr: 0x%x\n", g_kmap.kernel.rodata_start_addr, g_kmap.kernel.rodata_end_addr);
    console_printf("bss_start_addr: 0x%x, bss_end_addr: 0x%x\n", g_kmap.kernel.bss_start_addr, g_kmap.kernel.bss_end_addr);
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
    console_printf("%s%s%s", msg, buf, end);
}

static void test_vesa() {
    int ret = vesa_init(800, 600, 32);
    if (ret < 0) {
        console_printf("failed to init vesa graphics\n");
    }
    if (ret == 1) {
        // scroll to top
        for(int i = 0; i < MAXIMUM_PAGES; i++)
            console_scroll(SCROLL_UP);

        while (1) {
            // add scrolling to view all modes
            char c = kb_get_scancode();
            if (c == SCAN_CODE_KEY_UP)
                console_scroll(SCROLL_UP);
            if (c == SCAN_CODE_KEY_DOWN)
                console_scroll(SCROLL_DOWN);
        }
    } else {
        // fill some colors
        uint32 x = 0;
        for (uint32 c = 0; c < 267; c++) {
            for (uint32 i = 0; i < 600; i++) {
                vbe_putpixel(x, i, VBE_RGB(c % 255, 0, 0));
            }
            x++;
        }
        for (uint32 c = 0; c < 267; c++) {
            for (uint32 i = 0; i < 600; i++) {
                vbe_putpixel(x, i, VBE_RGB(0, c % 255, 0));
            }
            x++;
        }
        for (uint32 c = 0; c < 267; c++) {
            for (uint32 i = 0; i < 600; i++) {
                vbe_putpixel(x, i, VBE_RGB(0, 0, c % 255));
            }
            x++;
        }
    }
}
void yuri()
{
    while (1)
    {
        console_printf("Yuri is Peak!\n");
        usleep(100000);
        if (kbhit())
        {
            char c = kb_getchar();
            if (c == 'q')
                break;
        }
    }
    
}

void haiku()
{
    const char *five_syllable_lines[] = {
        "An old silent pond",
        "Autumn moonlight",
        "In the cicada's cry",
        "A world of dew",
        "The light of a star"
    };

    const char *seven_syllable_lines[] = {
        "A frog jumps into the pond",
        "Shadows stretch across the lake",
        "The summer grass whispers soft",
        "Dew drops glisten in the sun",
        "Waves crashing against the rocks"
    };

    size_t five_count = sizeof(five_syllable_lines) / sizeof(five_syllable_lines[0]);
    size_t seven_count = sizeof(seven_syllable_lines) / sizeof(seven_syllable_lines[0]);

    // Generate random indices
    size_t index1 = rand() % five_count;
    size_t index2 = rand() % seven_count;
    size_t index3 = rand() % five_count;

    console_printf("here is a haiku:\n");
    console_printf("%s\n", five_syllable_lines[index1]);
    console_printf("%s\n", seven_syllable_lines[index2]);
    console_printf("%s\n", five_syllable_lines[index3]);
}
void shell()
{
    char buffer[255];
    const char *shell = "$ ";

    while (1)
    {
        console_printf(shell);
        memset(buffer, 0, sizeof(buffer));
        getstr_bound(buffer, strlen(shell));
        if (strlen(buffer) == 0)
            continue;
        if (strcmp(buffer, "cpuid") == 0)
        {
            cpuid_info(1);
        }
        else if (strcmp(buffer, "haiku") == 0)
        {
            haiku();
        }
        else if (strcmp(buffer, "help") == 0)
        {
            console_printf("Hal OS Terminal\n");
            console_printf("--------------------------------------------------------------\n");
            console_printf("Available Commands:\n");
            console_printf("--------------------------------------------------------------\n");
            console_printf("  help      - Display this help message\n");
            console_printf("  cpuid     - Display CPU information\n");
            console_printf("  haiku     - Display a haiku\n");
            console_printf("  echo      - Echo a message to the console\n");
            console_printf("  clear     - Clear the console screen\n");
            console_printf("  memory    - Display system memory information\n");
            console_printf("  timer     - Display system timer information\n");
            console_printf("  shutdown  - Shut down the system\n");
            console_printf("  snake     - Play a game of Snake\n");
            console_printf("  vesa      - Display VESA graphics information\n");
            console_printf("  version   - Display Hal OS version information\n");
            console_printf("  yuri      - Display Yuri is Peak!\n");
            console_printf("--------------------------------------------------------------\n");
        }
        else if (strcmp(buffer, "help /f") == 0)
        {
            console_printf("help, cpuid, haiku, echo, clear, memory, timer, shutdown, snake, vesa, version, yuri\n");
        }
        else if (is_echo(buffer))
        {
            console_printf("%s\n", buffer + 5);
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
        else if (strcmp(buffer, "tss") == 0)
        {
            tss_print();
        }
        else if (strcmp(buffer, "vesa") == 0)
        {
            test_vesa();
        }
        else if (strcmp(buffer, "version") == 0)
        {
            console_printf("--------------------------------------------------------------\n");
            console_printf("Hal OS v0.5.0\n");
            console_printf("Built on: %s %s\n", __DATE__, __TIME__);
            console_printf("Built with: GCC %s\n", __VERSION__);
            console_printf("--------------------------------------------------------------\n");
        }
        else if (strcmp(buffer, "yuri") == 0)
        {
            yuri();
        }
        else
        {
            console_printf("invalid command: %s\n", buffer);
        }
    }
}