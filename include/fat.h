#ifndef FAT_H
#define FAT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


#define SECTOR_SIZE 512
#define FAT32_IS_DIR 0x10

// Add these error codes
#define FAT32_ERROR_NO_DEVICE    1
#define FAT32_ERROR_READ_FAILED  2
#define FAT32_ERROR_NO_FAT32     3
#define FAT32_ERROR_BAD_FORMAT   4
#define FAT32_ERROR_NO_MEMORY    5

#pragma pack(push, 1)

typedef struct {
  uint8_t bootjmp[3];
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_count;
  uint16_t root_dir_entries;
  uint16_t total_sectors_16;
  uint8_t media_type;
  uint16_t fat_size_16;
  uint16_t sectors_per_track;
  uint16_t head_count;
  uint32_t hidden_sectors;
  uint32_t total_sectors_32;
  uint32_t fat_size_32;
  uint16_t flags;
  uint16_t version;
  uint32_t root_cluster;
  uint16_t info_sector;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint8_t boot_signature2;
  uint8_t volume_id[4];
  uint8_t volume_label[11];
  uint8_t fs_type[8];
} FAT32_Header;

typedef struct {
    uint8_t short_name[8];
    uint8_t short_ext[3];
    uint8_t attrib;
    uint8_t lowercase;
    uint8_t uhhhhhhh;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t cluster_low;
    uint32_t file_size; // bytes
} FAT32_Directory_Entry;

typedef struct {
    uint8_t sequence;
    uint16_t name0[5];
    uint8_t attrib;
    uint8_t type;
    uint8_t checksum;
    uint16_t name1[6];
    uint16_t cluster;
    uint16_t name2[2];
} FAT32_Directory_Entry_LFN;

// static_assert(sizeof(FAT32_Directory_Entry) == 32, "wrong byte size!");
// static_assert(sizeof(FAT32_Directory_Entry) == sizeof(FAT32_Directory_Entry_LFN), "wrong byte size!");

#pragma pack(pop)

typedef struct {
  uint32_t attrib;
    uint32_t cluster;
    uint32_t size;
    uint32_t offset; // not used by any fat32_* funcs
} FAT32_File;

typedef uint32_t fat32_entry;

typedef struct {
    FAT32_Header header;
    uint32_t fat_size_in_sectors;
    uint32_t data_start_sector;
    uint32_t partition_start;  
    fat32_entry *fat;
    uint32_t cluster_size; // in bytes
} FAT32_Volume;

typedef struct {
  uint32_t cluster;
    uint32_t entry; // in cluster

    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t buffered_sector;
} FAT32_DirList;

void fat32_init_volume(FAT32_Volume* volume);
bool fat32_find_file(FAT32_Volume* volume, const char* path, FAT32_File* out_file);
void fat32_read_file(FAT32_Volume* volume, FAT32_File* file, uint8_t* out_buffer, uint32_t num_bytes, uint32_t start_offset);
void fat32_list_dir(FAT32_Volume* volume, FAT32_File* dir, FAT32_DirList* dir_list);
bool fat32_next_dir_entry(FAT32_Volume* volume, FAT32_DirList* dir_list, FAT32_File* out_file, char out_name[256]);
void fat32_unmount_volume(FAT32_Volume* volume);


#endif // FAT_H
