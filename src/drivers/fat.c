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

static uint8_t g_fat32_drive = 0;

static FAT32_Directory_Entry *read_next_entry(FAT32_Volume *volume, FAT32_DirList *dir_list);

int fat32_strcasecmp(const char *s1, const char *s2)
{
    while (1)
    {
        unsigned char c1 = toupper(*s1);
        unsigned char c2 = toupper(*s2);
        if (c1 != c2)
            return c1 - c2;
        if (c1 == '\0')
            return 0;
        s1++;
        s2++;
    }
}
static bool disk_read_sector(uint8_t *buffer, uint32_t sector)
{
    uint32_t timeout = 100000;
    while (timeout-- > 0)
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
static uint32_t cluster_to_sector(const FAT32_Volume *volume, uint32_t cluster)
{
    return volume->data_start_sector + (cluster - 2) * volume->header.sectors_per_cluster;
}

bool fat32_read_file(FAT32_Volume *volume, FAT32_File *file, uint32_t offset, uint8_t *buffer, uint32_t size)
{
    if (!volume || !file || !buffer)
    {
        serial_printf("[FAT32] Invalid parameters in fat32_read_file\n");
        return false;
    }

    if (offset + size > file->size && !(file->attrib & FAT32_IS_DIR))
    {
        serial_printf("[FAT32] Attempted read beyond file size\n");
        return false;
    }

    FAT32_Header *header = &volume->header;

    uint32_t cluster = file->cluster;
    uint32_t clusters_to_advance = offset / volume->cluster_size;
    for (uint32_t i = 0; i < clusters_to_advance; i++)
    {
        offset -= volume->cluster_size;
        cluster = volume->fat[cluster] & 0xFFFFFF;
        if (cluster == 0xFFFFFF)
        {
            serial_printf("[FAT32] Invalid cluster chain\n");
            return false;
        }
    }

    uint32_t bytes_left = size;
    while (true)
    {
        for (int i = offset / SECTOR_SIZE; i < header->sectors_per_cluster; i++)
        {
            offset %= SECTOR_SIZE;

            uint8_t sector_buffer[SECTOR_SIZE];
            if (!disk_read_sector(sector_buffer, cluster_to_sector(volume, cluster) + i))
            {
                return false;
            }

            uint32_t bytes_to_read = bytes_left;
            if (bytes_left + offset > SECTOR_SIZE)
            {
                bytes_to_read = SECTOR_SIZE - offset;
            }

            memcpy(buffer, sector_buffer + offset, bytes_to_read);
            bytes_left -= bytes_to_read;
            buffer += bytes_to_read;

            offset = 0;
            if (bytes_left == 0)
                return true;
        }

        cluster = volume->fat[cluster] & 0xFFFFFF;
        if (cluster == 0xFFFFFF)
            break;
    }

    return false;
}

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

#define LFN_CHARS_PER_ENTRY 13

static void parse_lfn_entry(FAT32_Directory_Entry_LFN *lfn, char *lfn_buffer)
{
    uint8_t seq = lfn->sequence & 0x1F;
    uint16_t chars[13];

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

void fat32_init_volume(FAT32_Volume *volume)
{
    if (!volume)
    {
        serial_printf("[FAT32] Invalid volume pointer\n");
        return;
    }

    memset(volume, 0, sizeof(FAT32_Volume));
    uint8_t *sector_buffer = dma_alloc(SECTOR_SIZE);
    if (!sector_buffer) {
        // Fallback to regular allocation if DMA fails
        serial_printf("[FAT32] DMA allocation failed, trying regular allocation\n");
        sector_buffer = vmm_alloc_page();
        if (!sector_buffer) {
            serial_printf("[FAT32] Failed to allocate sector buffer\n");
            return;
        }
    }

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

    if (!disk_read_sector(sector_buffer, 0))
    {
        serial_printf("[FAT32] Failed to read boot sector\n");
        return;
    }

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

    memcpy(&volume->header, sector_buffer, sizeof(FAT32_Header));
    const FAT32_Header *header = &volume->header;

    if (header->bytes_per_sector != SECTOR_SIZE)
    {
        serial_printf("[FAT32] Unsupported sector size: %u\n", header->bytes_per_sector);
        return;
    }

    volume->fat_size_in_sectors = header->fat_size_16 == 0 ? header->fat_size_32 : header->fat_size_16;

    volume->data_start_sector = header->reserved_sectors +
                                (header->fat_count * volume->fat_size_in_sectors);

    if (!sector_buffer)
    {
        serial_printf("[FAT32] DMA buffer allocation failed\n");
        return;
    }

    if (!disk_read_sector(sector_buffer, 0))
    {
        serial_printf("[FAT32] Read failed\n");
        dma_free(sector_buffer, SECTOR_SIZE);
        return;
    }

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

    volume->cluster_size = header->sectors_per_cluster * SECTOR_SIZE;

    serial_printf("[FAT32] Initialized successfully\n");
    serial_printf("  Sectors/cluster: %u\n", header->sectors_per_cluster);
    serial_printf("  FAT size: %u sectors\n", volume->fat_size_in_sectors);
    serial_printf("  Root cluster: %u\n", header->root_cluster);
}

bool fat32_find_file(FAT32_Volume *volume, const char *path, FAT32_File *out_file)
{
    if (!volume || !path || !out_file)
    {
        return false;
    }

    char component[256];
    uint32_t start = 0;
    size_t path_len = strlen(path);

    if (path_len == 0)
    {
        return false;
    }

    if (path[0] == '/')
        start = 1;

    FAT32_File current = {
        .cluster = volume->header.root_cluster,
        .attrib = FAT32_IS_DIR};

    while (start < path_len)
    {
        uint32_t end = start;
        while (path[end] != '\0' && path[end] != '/')
        {
            end++;
        }

        if (end == start)
            break;

        strncpy(component, path + start, end - start);
        component[end - start] = '\0';

        char component_upper[256];
        for (int i = 0; component[i]; i++)
        {
            component_upper[i] = toupper(component[i]);
        }
        component_upper[strlen(component)] = '\0';

        FAT32_DirList list;
        FAT32_File entry;
        char name[256];
        bool found = false;

        fat32_list_dir(volume, &current, &list);
        while (fat32_next_dir_entry(volume, &list, &entry, name))
        {
            if (fat32_strcasecmp(name, component_upper) == 0)
            {
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
            lfn_length = 0;
            return false;
        }

        if (entry->attrib == DIR_ENTRY_ATTRIB_LFN)
        {
            FAT32_Directory_Entry_LFN *lfn = (FAT32_Directory_Entry_LFN *)entry;
            uint8_t seq = lfn->sequence & 0x1F;

            if (seq == 0)
                continue;
            if (seq > lfn_length)
                lfn_length = seq;

            parse_lfn_entry(lfn, lfn_buffer);
        }
        else
        {
            if (lfn_length > 0)
            {
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

void fat32_unmount_volume(FAT32_Volume *volume)
{
    if (!volume)
    {
        serial_printf("[FAT32] Invalid volume pointer in unmount\n");
        return;
    }

    if (volume->fat)
    {
        if (volume->fat_size_in_sectors == 0)
        {
            serial_printf("[FAT32] Invalid FAT size in unmount\n");
            return;
        }
        dma_free(volume->fat, volume->fat_size_in_sectors * SECTOR_SIZE);
        volume->fat = NULL;
    }

    memset(volume, 0, sizeof(FAT32_Volume));
}
