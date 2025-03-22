#include "shell.h"

#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "string.h"
#include <kernel.h>
#include "io.h"
#include "timer.h"
#include "snake.h"
#include "vesa.h"
#include "keyboard.h"
#include "tss.h"
#include "liballoc.h"
#include "pmm.h"
#include "pci.h"
#include "ide.h"
#include "fat.h"
#include "arp.h"
#include "math.h"

#define BRAND_QEMU 1
#define BRAND_VBOX 2

KERNEL_MEMORY_MAP g_kmap;

extern uint32_t g_width;
extern uint32_t g_height;


extern IDE_DEVICE g_ide_devices[MAXIMUM_IDE_DEVICES];

typedef struct
{
    int x, y;
    int dx, dy;
    uint32_t color;
    int lifetime;
} Particle;

void __cpuid(uint32_t type, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "0"(type)); // put the type into eax
}

int cpuid_info(int print)
{
    char brand[49];
    uint32_t eax, ebx, ecx, edx;
    uint32_t type;

    memset(brand, 0, sizeof(brand));
    __cpuid(0x80000002, (uint32_t *)brand + 0x0, (uint32_t *)brand + 0x1, (uint32_t *)brand + 0x2, (uint32_t *)brand + 0x3);
    __cpuid(0x80000003, (uint32_t *)brand + 0x4, (uint32_t *)brand + 0x5, (uint32_t *)brand + 0x6, (uint32_t *)brand + 0x7);
    __cpuid(0x80000004, (uint32_t *)brand + 0x8, (uint32_t *)brand + 0x9, (uint32_t *)brand + 0xa, (uint32_t *)brand + 0xb);

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
void echo()
{
    console_printf("echo> ");

    // Declare and clear the input buffer
    char buffer[255];
    memset(buffer, 0, sizeof(buffer));

    // Get input from the user
    getstr(buffer, sizeof(buffer)); // Use the buffer size directly

    // Print the input back to the console
    console_printf("%s\n", buffer);
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

    console_printf("total_memory: %d MB, %d Bytes\n", g_kmap.system.total_memory / 1024, g_kmap.available.size);
    console_printf("kernel: %d MB, %d Bytes\n", g_kmap.kernel.k_len / 1024, g_kmap.kernel.k_len);
    console_printf("Used: %d MB, %d Bytes\n", pmm_used_blocks * PMM_BLOCK_SIZE / 1024 / 1024, pmm_used_blocks * PMM_BLOCK_SIZE);
    console_printf("Free: %d MB, %d Bytes\n", (g_kmap.available.size - pmm_used_blocks * PMM_BLOCK_SIZE) / 1024 / 1024, g_kmap.available.size - pmm_used_blocks * PMM_BLOCK_SIZE);
}

void ftoa(char *buf, float f)
{
    uint32_t count = 1;
    const uint32_t DEFAULT_DECIMAL_COUNT = 8;
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
            uint32_t y = decimal * 10; // y = 0.14159 * 10 = 1
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

static void test_vesa()
{
    console_printf("Running VESA graphics demo...\n");

    // Rainbow gradient - using triangle wave pattern instead of sine
    for (uint32_t y = 0; y < g_height; y++)
    {
        uint8_t r = (y * 255) / g_height;
        uint8_t g = ((y * 510 / g_height) > 255) ? 510 - (y * 510 / g_height) : (y * 510 / g_height);
        uint8_t b = 255 - ((y * 255) / g_height);
        for (uint32_t x = 0; x < g_width; x++)
        {
            vbe_putpixel(x, y, (r << 16) | (g << 8) | b);
        }
    }

    // Animated bouncing ball
    int ball_x = g_width / 2;
    int ball_y = g_height / 2;
    int dx = 5, dy = 3;
    int radius = 20;
    int iterations = 5000;

    while (iterations-- && !kbhit())
    {
        // Clear previous ball position
        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                if (x * x + y * y <= radius * radius)
                {
                    int px = ball_x + x;
                    int py = ball_y + y;
                    if (px >= 0 && px < g_width && py >= 0 && py < g_height)
                    {
                        // Restore background gradient using same calculation as above
                        uint8_t r = (py * 255) / g_height;
                        uint8_t g = ((py * 510 / g_height) > 255) ? 510 - (py * 510 / g_height) : (py * 510 / g_height);
                        uint8_t b = 255 - ((py * 255) / g_height);
                        vbe_putpixel(px, py, (r << 16) | (g << 8) | b);
                    }
                }
            }
        }

        // Update ball position
        ball_x += dx;
        ball_y += dy;

        // Bounce off edges
        if (ball_x <= radius || ball_x >= g_width - radius)
            dx = -dx;
        if (ball_y <= radius || ball_y >= g_height - radius)
            dy = -dy;

        // Draw new ball position
        for (int y = -radius; y <= radius; y++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                if (x * x + y * y <= radius * radius)
                {
                    int px = ball_x + x;
                    int py = ball_y + y;
                    if (px >= 0 && px < g_width && py >= 0 && py < g_height)
                    {
                        // Create a shiny effect
                        int d = (x * x + y * y);
                        int intensity = 255 - (d * 255) / (radius * radius);
                        vbe_putpixel(px, py, (intensity << 16) | (intensity << 8) | intensity);
                    }
                }
            }
        }

        // Swap buffers so that the frame becomes visible
        vesa_swap_buffers();
        usleep(20000); // Add a small delay
    }

    console_printf("Press any key to exit VESA demo...\n");
    if (!kbhit())
        kb_getchar();
    console_clear();
}

void haiku()
{
    const char *five_syllable_lines[] = {
        "An old silent pond",
        "Autumn moonlight",
        "In the cicada's cry",
        "A world of dew",
        "The light of a star"};

    const char *seven_syllable_lines[] = {
        "A frog jumps into the pond",
        "Shadows stretch across the lake",
        "The summer grass whispers soft",
        "Dew drops glisten in the sun",
        "Waves crashing against the rocks"};

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

int test_memory_allocation()
{
    void *ptr = malloc(64);
    if (ptr == NULL)
    {
        console_printf("Initial malloc allocation failed\n");
        return -1;
    }

    // Expand
    void *ptr2 = realloc(ptr, 128);
    if (ptr2 == NULL)
    {
        console_printf("Realloc expansion failed\n");
        free(ptr);
        return -1;
    }

    // Shrink
    void *ptr3 = realloc(ptr2, 32);
    if (ptr3 == NULL)
    {
        console_printf("Realloc shrink failed\n");
        free(ptr2);
        return -1;
    }

    free(ptr3);
    console_printf("Memory allocation test passed\n");
    return 0;
}

// void drive_read(int drive, uint32_t num_sectors, uint32_t lba, char *buffer)
// {
//     ide_read_sectors(drive, num_sectors, lba, (uint32_t)buffer);
//     console_printf("Reading %u sectors from drive %d at LBA %u\n", num_sectors, drive, lba);
// }

// void drive_write(int drive, uint32_t num_sectors, uint32_t lba, char *buffer)
// {
//     ide_write_sectors(drive, num_sectors, lba, (uint32_t)buffer);
//     console_printf("Writing %u sectors to drive %d at LBA %u from buffer %s\n", num_sectors, drive, lba, buffer);
// }
// void drive_list()
// {
//     console_printf("List of Drives:\n");
//     for (int i = 0; i < MAXIMUM_IDE_DEVICES; i++)
//     {
//         if (g_ide_devices[i].reserved)
//         {
//             console_printf("  %s\n", g_ide_devices[i].model);
//         }
//     }
// }

// void drive()
// {
//     char buffer[255];
//     char buf[ATA_SECTOR_SIZE] = {0};
//     int drive = -1;
//     uint32_t lba = 0;
//     uint8_t no_of_sectors = 1;

//     console_printf("Drive Menu\n");
//     console_printf("Type help for help\n");
//     const char *shell = "drive> ";
//     const char *read = "read> ";
//     const char *write = "write> ";
//     const char *select = "select> ";

//     while (1)
//     {
//         console_printf(shell);
//         memset(buffer, 0, sizeof(buffer));
//         getstr(buffer, sizeof(buffer));
//         if (strcmp(buffer, "exit") == 0)
//             break;
//         else if (strcmp(buffer, "help") == 0)
//         {
//             console_printf("=====================================\n");
//             console_printf("|        Drive Menu Help Guide      |\n");
//             console_printf("=====================================\n");
//             console_printf("|  Commands:                        |\n");
//             console_printf("|  * read   : Read from Drive       |\n");
//             console_printf("|  * write  : Write to Drive        |\n");
//             console_printf("|  * list   : List of Drives        |\n");
//             console_printf("|  * select : Select a Drive        |\n");
//             console_printf("|  * info   : Get Info of Drive     |\n");
//             console_printf("|  * clear  : Clear Screen          |\n");
//             console_printf("|  * exit   : Exit from Drive Menu  |\n");
//             console_printf("=====================================\n");
//         }
//         else if (strcmp(buffer, "read") == 0)
//         {
//             if (drive == -1)
//             {
//                 console_printf("Please select a drive first\n");
//                 continue;
//             }
//             while (1)
//             {
//                 console_printf(read);
//                 memset(buf, 0, sizeof(buf));
//                 getstr(buffer, sizeof(buffer)); // Get the read command input
//                 if (strcmp(buffer, "exit") == 0)
//                     break;

//                 // Perform the read operation
//                 drive_read(drive, no_of_sectors, lba, buf);

//                 // Display the read contents
//                 console_printf("Data read: %s\n", buf);
//                 break;
//             }
//         }
//         else if (strcmp(buffer, "write") == 0)
//         {
//             if (drive == -1)
//             {
//                 console_printf("Please select a drive first\n");
//                 continue;
//             }
//             while (1)
//             {
//                 console_printf(write);
//                 memset(buf, 0, sizeof(buf));
//                 getstr(buf, sizeof(buf)); // Get the string input into buf
//                 if (strcmp(buf, "exit") == 0)
//                     break;

//                 if (strlen(buf) > ATA_SECTOR_SIZE)
//                 {
//                     console_printf("Error: Input exceeds sector size (%d bytes)\n", ATA_SECTOR_SIZE);
//                     continue;
//                 }

//                 // Write the contents of buf to the drive
//                 drive_write(drive, no_of_sectors, lba, buf);
//                 console_printf("Data written successfully: %s\n", buf);
//                 break;
//             }
//         }
//         else if (strcmp(buffer, "clear") == 0)
//         {
//             console_clear();
//         }
//         else if (strcmp(buffer, "list") == 0)
//         {
//             console_printf("List of Drives:\n");
//             for (int i = 0; i < MAXIMUM_IDE_DEVICES; i++)
//             {
//                 if (g_ide_devices[i].reserved)
//                 {
//                     console_printf("  %s\n", g_ide_devices[i].model);
//                 }
//             }
//             console_printf("List of all Drives:\n");
//             for (int i = 0; i < 4; i++)
//             {
//                 if (g_ide_devices[i].reserved == 1)
//                 {
//                     console_printf("%d:-\n", i);
//                     console_printf("  model: %s\n", g_ide_devices[i].model);
//                     console_printf("  type: %s\n", (const char *[]){"ATA", "ATAPI"}[g_ide_devices[i].type]);
//                     console_printf("  drive: %d, channel: %d\n", g_ide_devices[i].drive, g_ide_devices[i].channel);
//                     console_printf("  base: 0x%x, control: 0x%x\n", g_ide_channels[i].base, g_ide_channels[i].control);
//                     console_printf("  size: %d sectors, %d bytes\n", g_ide_devices[i].size, g_ide_devices[i].size * ATA_SECTOR_SIZE);
//                     console_printf("  signature: 0x%x, features: %d\n", g_ide_devices[i].signature, g_ide_devices[i].features);
//                 }
//             }
//         }
//         else if (strcmp(buffer, "info") == 0)
//         {
//             if (drive == -1)
//             {
//                 console_printf("Please select a drive first\n");
//                 continue;
//             }
//             console_printf("Drive %d:\n", drive);
//             console_printf("  Model: %s\n", g_ide_devices[drive].model);
//             console_printf("  Type: %s\n", g_ide_devices[drive].type ? "ATAPI" : "ATA");
//             console_printf("  Size: %d bytes\n", g_ide_devices[drive].size);
//         }
//         else if (strcmp(buffer, "select") == 0)
//         {
//             console_printf(select);
//             memset(buffer, 0, sizeof(buffer));
//             getstr(buffer, sizeof(buffer));
//             int selected_drive = atoi(buffer);
//             if (selected_drive >= 0 && selected_drive < MAXIMUM_IDE_DEVICES && g_ide_devices[selected_drive].reserved)
//             {
//                 drive = selected_drive;
//                 console_printf("Drive %d selected\n", drive);
//             }
//             else
//             {
//                 console_printf("Invalid drive selection\n");
//             }
//         }
//         else
//         {
//             console_printf("Invalid command: %s\n", buffer);
//         }
//     }
// }

// void drive_test()
// {
//     console_printf("Drive Test:\n");
// }

void hwinfo()
{
    console_printf("Hardware Information:\n");
    console_printf("CPU: ");
    cpuid_info(1);
    console_printf("Memory: %d MB\n", g_kmap.system.total_memory / 1024);
    console_printf("Screen Resolution: %dx%d\n", g_width, g_height);
    // console_printf("Drives:\n");
    // drive_list();
}
void fireworks()
{
    Particle particles[1000];
    int active = 0;

    while (!kbhit())
    {
        if (active < 900 && (rand() % 10 == 0))
        {
            int base_x = rand() % g_width;
            int base_y = g_height;
            uint32_t color = (rand() % 255 << 16) | (rand() % 255 << 8) | rand() % 255;
            
            for (int i = 0; i < 100; i++)
            {
                float angle = (float)i * 3.14159 * 2 / 50;
                float speed = (rand() % 1000) / 100.0f + 3;
                
                particles[active] = (Particle){
                    base_x, base_y,
                    (int)(cos(angle) * speed),
                    (int)(sin(angle) * speed) - 10,
                    color,
                    70 + (rand() % 30)
                };
                active++;
            }
        }

        for (int i = 0; i < active; i++)
        {
            if (particles[i].x >= 0 && particles[i].x < g_width &&
                particles[i].y >= 0 && particles[i].y < g_height)
            {
                vbe_putpixel(particles[i].x, particles[i].y, 0);
            }
        }

        for (int i = 0; i < active; i++)
        {
            particles[i].x += particles[i].dx;
            particles[i].y += particles[i].dy;
            particles[i].dy += 0.2;
            particles[i].lifetime--;

            uint32_t r = ((particles[i].color >> 16) & 0xFF) * particles[i].lifetime / 50;
            uint32_t g = ((particles[i].color >> 8) & 0xFF) * particles[i].lifetime / 50;
            uint32_t b = (particles[i].color & 0xFF) * particles[i].lifetime / 50;
            uint32_t fade_color = (r << 16) | (g << 8) | b;

            if (particles[i].x >= 0 && particles[i].x < g_width &&
                particles[i].y >= 0 && particles[i].y < g_height)
            {
                vbe_putpixel(particles[i].x, particles[i].y, fade_color);
            }

            if (particles[i].lifetime <= 0)
            {
                particles[i] = particles[active - 1];
                active--;
                i--;
            }
        }
        usleep(20000);
        vesa_swap_buffers();
    }
    console_clear();
}

void ls()
{
    FAT32_Volume volume;
    fat32_init_volume(&volume);

    FAT32_File root;
    // Find the root directory ("/")
    if (fat32_find_file(&volume, "/", &root))
    {
        FAT32_DirList dir_list;
        char name[256];
        FAT32_File entry;

        // Start directory listing
        fat32_list_dir(&volume, &root, &dir_list);

        // Iterate through directory entries
        console_printf("Contents of root directory:\n");
        while (fat32_next_dir_entry(&volume, &dir_list, &entry, name))
        {
            // Display entry type (file/directory) and name
            console_printf(" [%s] %s (%d bytes)\n",
                           (entry.attrib & FAT32_IS_DIR) ? "DIR" : "FILE",
                           name,
                           entry.size);
        }
    }
    else
    {
        console_printf("Error: Could not find root directory\n");
    }

    fat32_unmount_volume(&volume); // Cleanup
}

void show_arp_cache()
{
    console_printf("ARP Cache Table:\n");
    console_printf("-----------------------------------------------\n");
    console_printf("IP Address        MAC Address           Age\n");
    console_printf("-----------------------------------------------\n");

    uint32_t current_time = get_ticks();

    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (arp_cache[i].ip != 0)
        {
            // Format IP address
            console_printf("%d.%d.%d.%d    ",
                           (arp_cache[i].ip >> 24) & 0xFF,
                           (arp_cache[i].ip >> 16) & 0xFF,
                           (arp_cache[i].ip >> 8) & 0xFF,
                           arp_cache[i].ip & 0xFF);

            // Format MAC address
            console_printf("%02x:%02x:%02x:%02x:%02x:%02x    ",
                           arp_cache[i].mac[0], arp_cache[i].mac[1],
                           arp_cache[i].mac[2], arp_cache[i].mac[3],
                           arp_cache[i].mac[4], arp_cache[i].mac[5]);

            // Calculate age in seconds
            uint32_t age = (current_time - arp_cache[i].timestamp) / 100; // Since timer runs at 100Hz
            console_printf("%ds\n", age);
        }
    }
    console_printf("-----------------------------------------------\n");
}

void shell()
{
    serial_printf("[SHELL] Starting shell...\n");

    console_clear();
    // console_printf("DEBUG: Console initialized\n");  // Check if this appears
    console_printf("Hal OS v0.13.0\n");
    console_printf("Type 'help' for a list of commands\n");
    char buffer[255];
    const char *shell = "kernel> ";

    while (1)
    {
        console_printf(shell);
        console_refresh();
        memset(buffer, 0, sizeof(buffer));
        getstr(buffer, sizeof(buffer));
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
            console_printf("===============================================\n");
            console_printf("|              Hal OS Terminal                |\n");
            console_printf("===============================================\n");
            console_printf("|  Available Commands:                        |\n");
            console_printf("|   * arp - Display ARP cache                 |\n");
            console_printf("|   * clear - Clear the console screen        |\n");
            console_printf("|   * cpuid - Display CPU information         |\n");
            // console_printf("|   * drive - Read or write from/to a drive   |\n");
            console_printf("|   * echo - Echo a message to the console    |\n");
            console_printf("|   * haiku - Display a haiku                 |\n");
            console_printf("|   * help - Display this help message        |\n");
            console_printf("|   * hwinfo - Display hardware information   |\n");
            console_printf("|   * ls - List files in the root directory   |\n");
            console_printf("|   * lspci - Display PCI information         |\n");
            console_printf("|   * malloc - Test memory allocation         |\n");
            console_printf("|   * fireworks - Fireworks effect            |\n");
            console_printf("|   * memory - Display system memory          |\n");
            console_printf("|   * shutdown - Shut down the system         |\n");
            console_printf("|   * snake - Play a game of Snake            |\n");
            console_printf("|   * timer - Display system timer            |\n");
            console_printf("|   * vesa - Display VESA graphics            |\n");
            console_printf("|   * version - Display Hal OS version        |\n");
            console_printf("===============================================\n");
        }
        else if (strcmp(buffer, "help /f") == 0)
        {
            console_printf("clear, cpuid, echo, haiku, help, hwinfo, lspci, malloc, memory, shutdown, snake, timer, vesa, version\n");
        }
        else if (strcmp(buffer, "echo") == 0)
        {
            echo();
        }
        else if (strcmp(buffer, "arp") == 0)
        {
            show_arp_cache();
        }
        // else if (strcmp(buffer, "drive") == 0)
        // {
        //     drive();
        // }
        else if (strcmp(buffer, "ls") == 0)
        {
            ls();
        }
        else if (strcmp(buffer, "shutdown") == 0)
        {
            shutdown();
        }
        else if (strcmp(buffer, "clear") == 0)
        {
            console_clear();
        }
        else if (strcmp(buffer, "timer") == 0)
        {
            timer();
        }
        else if (strcmp(buffer, "malloc") == 0)
        {
            test_memory_allocation();
        }
        else if (strcmp(buffer, "memory") == 0)
        {
            memory();
        }
        else if (strcmp(buffer, "lspci") == 0)
        {
            pci_print_devices();
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
            console_printf("Hal OS v0.13.0\n");
            console_printf("Built on: %s %s\n", __DATE__, __TIME__);
            console_printf("Built with: GCC %s\n", __VERSION__);
            console_printf("--------------------------------------------------------------\n");
        }
        else if (strcmp(buffer, "hwinfo") == 0)
        {
            hwinfo();
        }
        else if (strcmp(buffer, "fireworks") == 0)
        {
            fireworks();
        }
        else
        {
            console_printf("invalid command: %s\n", buffer);
        }
    }
}
