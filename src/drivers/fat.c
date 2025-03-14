// fat 32 driver
#include "fat.h"

#include <stdint.h>
#include "string.h"
#include "io.h"
#include "liballoc.h"
#include "serial.h"
#include "vmm.h"

#include "ide.h"

#define DIR_ENTRY_ATTRIB_LFN 0x0F

extern IDE_DEVICE g_ide_devices[MAXIMUM_IDE_DEVICES];

// Drive number for FAT32 filesystem
static uint8_t g_fat32_drive = 0;

static FAT32_Directory_Entry *read_next_entry(FAT32_Volume *volume, FAT32_DirList *dir_list);

static int toupper(int c)
{
    return (c >= 'a' && c <= 'z') ? (c & ~32) : c;
}

static int tolower(int c)
{
    return (c >= 'A' && c <= 'Z') ? (c | 32) : c;
}
int fat32_strcasecmp(const char *s1, const char *s2) {
    while (1) {
        unsigned char c1 = toupper(*s1);
        unsigned char c2 = toupper(*s2);
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
        s1++;
        s2++;
    }
}
static bool disk_read_sector(uint8_t *buffer, uint32_t sector)
{
    uint32_t timeout = 100000;
    while(timeout-- > 0)
    {

        if (ide_read_sectors(g_fat32_drive, 1, sector, (uint32_t)buffer) != 0)
        {
            serial_printf("[FAT32] Error reading sector %u\n", sector);
            return false;
        }
        return true;
    }
    serial_printf("[FAT32] Error Read timeout at sector %u\n", sector);
    return false;
}

static bool disk_write_sector(uint8_t *buffer, uint32_t sector)
{
    if (ide_write_sectors(g_fat32_drive, 1, sector, (uint32_t)buffer) != 0)
    {
        serial_printf("[FAT32] Error writing sector %u\n", sector);
        return false;
    }
    return true;
}

static void split_path(const char* path, char* parent, char* filename) {
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        strcpy(parent, "/");
        strcpy(filename, path);
        return;
    }

    strncpy(parent, path, last_slash - path);
    parent[last_slash - path] = '\0';
    strcpy(filename, last_slash + 1);
}

static void to_short_filename(const char* name, char* short_name, char* short_ext) {
    memset(short_name, ' ', 8);
    memset(short_ext, ' ', 3);

    const char* dot = strchr(name, '.');
    if (!dot) {
        strncpy(short_name, name, 8);
        return;
    }

    strncpy(short_name, name, dot - name < 8 ? dot - name : 8);
    strncpy(short_ext, dot + 1, strlen(dot + 1) < 3 ? strlen(dot + 1) : 3);

    // Convert to uppercase
    for (int i = 0; i < 8; i++) short_name[i] = toupper(short_name[i]);
    for (int i = 0; i < 3; i++) short_ext[i] = toupper(short_ext[i]);
}

static uint32_t cluster_to_sector(const FAT32_Volume *volume, uint32_t cluster)
{
    return volume->data_start_sector + (cluster - 2) * volume->header.sectors_per_cluster;
}


void fat32_read_file(FAT32_Volume *volume, FAT32_File *file, uint8_t *out_buffer, uint32_t num_bytes, uint32_t start_offset)
{
    if (!volume || !file || !out_buffer)
    {
        serial_printf("[FAT32] Invalid parameters in fat32_read_file\n");
        return;
    }

    if (start_offset + num_bytes > file->size && !(file->attrib & FAT32_IS_DIR))
    {
        serial_printf("[FAT32] Attempted read beyond file size\n");
        return;
    }

    FAT32_Header *header = &volume->header;

    uint32_t cluster = file->cluster;
    uint32_t clusters_to_advance = start_offset / volume->cluster_size;
    for (int i = 0; i < clusters_to_advance; i++)
    {
        start_offset -= volume->cluster_size;
        cluster = volume->fat[cluster] & 0xFFFFFF;
        if (cluster == 0xFFFFFF)
        {
            serial_printf("[FAT32] Invalid cluster chain\n");
            return;
        }
    }

    uint32_t bytes_left = num_bytes;
    while (true)
    {
        for (int i = start_offset / SECTOR_SIZE; i < header->sectors_per_cluster; i++)
        {
            start_offset %= SECTOR_SIZE;

            uint8_t buffer[SECTOR_SIZE];
            if (!disk_read_sector(buffer, cluster_to_sector(volume, cluster) + i))
            {
                return;
            }

            uint32_t bytes_to_read = bytes_left;
            if (bytes_left + start_offset > SECTOR_SIZE)
            {
                bytes_to_read = SECTOR_SIZE - start_offset;
            }

            memcpy(out_buffer, buffer + start_offset, bytes_to_read);
            bytes_left -= bytes_to_read;
            out_buffer += bytes_to_read;

            start_offset = 0;
            if (bytes_left == 0)
                return;
        }

        cluster = volume->fat[cluster] & 0xFFFFFF;
        if (cluster == 0xFFFFFF)
            break;
    }
}

// dos style 8.3
static void parse_short_filename(char output[13], FAT32_Directory_Entry *entry)
{
    strncpy(output, (const char *)entry->short_name, 8);
    int i;
    for (i = 7; i >= 0; i--)
    {
        if (output[i] != ' ')
        {
            break;
        }
        output[i] = '\0';
    }

    if (strncmp((char *)entry->short_ext, "   ", 3) == 0)
    {
        output[i + 1] = '\0';
        return;
    }

    output[i + 1] = '.';
    output[i + 2] = '\0';
    strncat(output, (const char *)entry->short_ext, 3);

    if (entry->lowercase)
    {
        for (i = 0; i < 13; i++)
        {
            output[i] = tolower(output[i]);
        }
    }
}

static char ucs2_to_ascii(uint16_t ucs2)
{
    return ucs2 & 0xFF;
}

#define LFN_CHARS_PER_ENTRY 13

static void parse_lfn_entry(FAT32_Directory_Entry_LFN *lfn, char *lfn_buffer)
{
    uint8_t seq = lfn->sequence & 0x1F; // Remove last-entry flag
    uint16_t chars[13];

    // Extract all 13 characters
    memcpy(chars, lfn->name0, 10);
    memcpy(chars + 5, lfn->name1, 12);
    memcpy(chars + 11, lfn->name2, 4);

    for (int i = 0; i < 13; i++)
    {
        if (chars[i] == 0xFFFF)
            break;
        lfn_buffer[(seq - 1) * 13 + i] = (chars[i] & 0xFF);
    }
}

// returns length in clusters
static uint32_t count_fat_chain_length(const FAT32_Volume *volume, uint32_t cluster)
{
    uint32_t length = 1;

    while (true)
    {
        cluster = volume->fat[cluster] & 0xFFFFFF;

        if (cluster == 0xFFFFFF)
            break;

        length++;
    }

    return length;
}

static uint32_t fat32_strncmp_nocase(const char *s1, const char *s2, uint32_t n)
{
    register unsigned char u1, u2;

    while (n-- > 0)
    {
        u1 = toupper((unsigned char)*s1++);
        u2 = toupper((unsigned char)*s2++);
        if (u1 != u2)
            return u1 - u2;
        if (u1 == '\0')
            return 0;
    }
    return 0;
}

static bool find_file_in_dir(FAT32_Volume *volume, FAT32_File *dir_file, const char *name, uint32_t name_len, FAT32_File *out_file)
{
    FAT32_DirList list;
    fat32_list_dir(volume, dir_file, &list);

    char entry_name[256];
    while (fat32_next_dir_entry(volume, &list, out_file, entry_name))
    {
        if (fat32_strncmp_nocase(entry_name, name, name_len) == 0)
        {
            return true;
        }
    }

    return false;
}

void fat32_init_volume(FAT32_Volume *volume)
{
    if (!volume)
    {
        serial_printf("[FAT32] Invalid volume pointer\n");
        return;
    }

    memset(volume, 0, sizeof(FAT32_Volume));
    uint8_t *sector_buffer = dma_alloc(SECTOR_SIZE);

    // 1. Find a valid IDE ATA drive
    g_fat32_drive = 0;
    while (g_fat32_drive < MAXIMUM_IDE_DEVICES)
    {
        if (g_ide_devices[g_fat32_drive].reserved &&
            g_ide_devices[g_fat32_drive].type == IDE_ATA)
        {
            break;
        }
        g_fat32_drive++;
    }
    serial_printf("[FAT32] Found drive %u\n", g_fat32_drive);

    if (g_fat32_drive >= MAXIMUM_IDE_DEVICES)
    {
        serial_printf("[FAT32] No suitable drive found\n");
        return;
    }

    // 2. Read boot sector (LBA 0)
    if (!disk_read_sector(sector_buffer, 0))
    {
        serial_printf("[FAT32] Failed to read boot sector\n");
        return;
    }

    // 3. Validate FAT32 signature and type
    uint8_t boot_sig1 = sector_buffer[510];
    uint8_t boot_sig2 = sector_buffer[511];
    if (boot_sig1 != 0x55 || boot_sig2 != 0xAA)
    {
        serial_printf("[FAT32] Invalid boot signature (0x%x%x)\n", boot_sig2, boot_sig1);
        return;
    }

    if (strncmp((char *)sector_buffer + 82, "FAT32   ", 8) != 0)
    {
        serial_printf("[FAT32] Not a FAT32 filesystem\n");
        return;
    }

    // 4. Copy header structure
    memcpy(&volume->header, sector_buffer, sizeof(FAT32_Header));
    const FAT32_Header *header = &volume->header;

    // 5. Validate sector size
    if (header->bytes_per_sector != SECTOR_SIZE)
    {
        serial_printf("[FAT32] Unsupported sector size: %u\n", header->bytes_per_sector);
        return;
    }

    // 6. Calculate FAT parameters
    volume->fat_size_in_sectors = header->fat_size_16 == 0 ? header->fat_size_32 : header->fat_size_16;

    volume->data_start_sector = header->reserved_sectors +
                                (header->fat_count * volume->fat_size_in_sectors);

    // 7. Allocate and read FAT table
    if (!sector_buffer)
    {
        serial_printf("[FAT32] DMA buffer allocation failed\n");
        return;
    }

    // Read boot sector
    if (!disk_read_sector(sector_buffer, 0))
    {
        serial_printf("[FAT32] Read failed\n");
        dma_free(sector_buffer, SECTOR_SIZE);
        return;
    }

    // Allocate FAT table with contiguous memory
    uint32_t fat_size = volume->fat_size_in_sectors * SECTOR_SIZE;
    if (volume->fat_size_in_sectors == 0)
    {
        serial_printf("[FAT32] FAT size is zero (invalid filesystem)\n");
        return;
    }
    volume->fat = (fat32_entry *)dma_alloc(fat_size);
    if (!volume->fat)
    {
        serial_printf("[FAT32] FAT table allocation failed\n");
        dma_free(sector_buffer, SECTOR_SIZE);
        return;
    }

    // 8. Final initialization
    volume->cluster_size = header->sectors_per_cluster * SECTOR_SIZE;

    serial_printf("[FAT32] Initialized successfully\n");
    serial_printf("  Sectors/cluster: %u\n", header->sectors_per_cluster);
    serial_printf("  FAT size: %u sectors\n", volume->fat_size_in_sectors);
    serial_printf("  Root cluster: %u\n", header->root_cluster);
}

bool fat32_find_file(FAT32_Volume *volume, const char *path, FAT32_File *out_file)
{
    char component[256];
    uint32_t start = 0;

    if (path[0] == '/')
        start = 1;

    FAT32_File current = {
        .cluster = volume->header.root_cluster,
        .attrib = FAT32_IS_DIR};

    while (1)
    {
        // Extract next path component
        uint32_t end = start;
        while (path[end] != '\0' && path[end] != '/')
        {
            end++;
        }

        if (end == start)
            break; // Empty component

        strncpy(component, path + start, end - start);
        component[end - start] = '\0';

        // Search directory
        FAT32_DirList list;
        FAT32_File entry;
        char name[256];
        bool found = false;

        fat32_list_dir(volume, &current, &list);
        while (fat32_next_dir_entry(volume, &list, &entry, name))
        {
            if (fat32_strcasecmp(name, component) == 0) {
                current = entry;
                found = true;
                break;
            }
        }

        if (!found)
            return false;
        if (!(current.attrib & FAT32_IS_DIR) && path[end] == '/')
            return false;

        start = end + 1;
    }

    *out_file = current;
    return true;
}

void fat32_list_dir(FAT32_Volume *volume, FAT32_File *dir, FAT32_DirList *dir_list)
{
    if (!volume || !dir || !dir_list)
    {
        serial_printf("[FAT32] Invalid parameters in fat32_list_dir\n");
        return;
    }

    if (!(dir->attrib & FAT32_IS_DIR))
    {
        serial_printf("[FAT32] Attempted to list non-directory\n");
        return;
    }

    dir_list->cluster = dir->cluster;
    dir_list->entry = 0;
    dir_list->buffered_sector = 0;
}

static FAT32_Directory_Entry *read_next_entry(FAT32_Volume *volume, FAT32_DirList *dir_list)
{
    uint32_t entries_per_cluster = volume->cluster_size / sizeof(FAT32_Directory_Entry);
    uint32_t entries_per_sector = SECTOR_SIZE / sizeof(FAT32_Directory_Entry);

    if (dir_list->entry >= entries_per_cluster)
    {
        dir_list->cluster = volume->fat[dir_list->cluster] & 0xFFFFFF;
        if (dir_list->cluster == 0xFFFFFF)
        {
            serial_printf("[FAT32] Invalid cluster in directory chain\n");
            return NULL;
        }
        dir_list->entry = 0;
    }

    uint32_t right_sector = cluster_to_sector(volume, dir_list->cluster) + dir_list->entry / entries_per_sector;
    if (dir_list->buffered_sector != right_sector)
    {
        if (!disk_read_sector(dir_list->sector_buffer, right_sector))
        {
            return NULL;
        }
        dir_list->buffered_sector = right_sector;
    }

    uint32_t entry_in_sector = dir_list->entry % entries_per_sector;
    FAT32_Directory_Entry *entry = (FAT32_Directory_Entry *)(dir_list->sector_buffer + entry_in_sector * sizeof(FAT32_Directory_Entry));
    dir_list->entry++;

    if (entry->short_name[0] == 0)
        return NULL;

    return entry;
}

bool fat32_next_dir_entry(FAT32_Volume *volume, FAT32_DirList *dir_list,
                          FAT32_File *out_file, char out_name[256])
{
    static char lfn_buffer[256];
    static uint8_t lfn_length = 0;

    while (1)
    {
        FAT32_Directory_Entry *entry = read_next_entry(volume, dir_list);
        if (!entry)
        {
            lfn_length = 0; // Reset on directory end
            return false;
        }

        if (entry->attrib == DIR_ENTRY_ATTRIB_LFN)
        {
            FAT32_Directory_Entry_LFN *lfn = (FAT32_Directory_Entry_LFN *)entry;
            uint8_t seq = lfn->sequence & 0x1F;

            if (seq == 0)
                continue; // Deleted entry
            if (seq > lfn_length)
                lfn_length = seq;

            parse_lfn_entry(lfn, lfn_buffer);
        }
        else
        {
            if (lfn_length > 0)
            {
                // Reverse LFN parts and trim nulls
                char temp[256];
                uint8_t parts = lfn_length;

                for (int i = 0; i < parts; i++)
                {
                    strncpy(temp + i * 13,
                            lfn_buffer + (parts - i - 1) * 13,
                            13);
                }
                temp[parts * 13] = '\0';
                strcpy(out_name, temp);
                lfn_length = 0;
            }
            else
            {
                parse_short_filename(out_name, entry);
            }

            out_file->attrib = entry->attrib;
            out_file->cluster = (entry->cluster_high << 16) | entry->cluster_low;
            out_file->size = entry->file_size;
            return true;
        }
    }
}

void fat32_unmount_volume(FAT32_Volume* volume) {
    if (volume->fat) {
        dma_free(volume->fat, volume->fat_size_in_sectors * SECTOR_SIZE);
        volume->fat = NULL;
    }
}