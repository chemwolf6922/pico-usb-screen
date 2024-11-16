#include "fat12.h"
#include <string.h>

#define BOOT_ENTRY_NUM (16)

typedef struct
{
    uint8_t jmp_boot[3];
    char oem_name[8];
    uint16_t byte_per_sector;
    uint8_t sector_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_num;
    uint16_t root_entry_num;
    uint16_t sector_num;
    uint8_t media_type;
    uint16_t sector_per_fat;
    uint16_t sector_per_head;
    uint16_t heads_per_cylinder;
    uint32_t hidden_sectors;
    uint32_t sector_num_big;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t extended_boot_signature;
    uint32_t volume_serial_number;
    char volume_label[11];
    char filesystem_type[8];
    uint8_t boot_code[448];
    uint16_t boot_sector_signature;
} __attribute__((packed)) fat_boot_sector_t ;

typedef struct
{
    char filename[11];
    struct
    {
        uint8_t read_only : 1;
        uint8_t hidden : 1;
        uint8_t system : 1;
        uint8_t volume_id : 1;
        uint8_t directory : 1;
        uint8_t archive : 1;
        uint8_t reserved : 2;
    } __attribute__((packed)) attribute;
    uint8_t reserved[2];
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint8_t reserved_fat32[2];
    uint16_t last_write_time;
    uint16_t last_write_date;
    uint16_t first_logical_cluster;
    uint32_t file_size;
} __attribute__((packed)) fat_directory_entry_t;

static const fat_boot_sector_t default_boot_sector = {
    .jmp_boot = {0xEB, 0x3C, 0x90},
    .oem_name = "MSDOS5.0",
    .byte_per_sector = DISK_BLOCK_SIZE,
    .sector_per_cluster = 1,
    .reserved_sectors = 1,
    .fat_num = 1,
    .root_entry_num = BOOT_ENTRY_NUM,
    .sector_num = DISK_BLOCK_NUM,
    .media_type = 0xF8,
    .sector_per_fat = 1,
    .sector_per_head = 1,
    .heads_per_cylinder = 1,
    .hidden_sectors = 0,
    .sector_num_big = 0,
    .drive_number = 0x80,
    .reserved = 0,
    .extended_boot_signature = 0x29,
    .volume_serial_number = 0x00001234,
    .volume_label = "USBSCREEN  ",
    .filesystem_type = "FAT12   ",
    .boot_code = {0},
    .boot_sector_signature = 0xAA55
};

static const uint8_t default_fat_entry[3] = {0xF8, 0xFF, 0xFF};

static const fat_directory_entry_t volume_label_entry = {
    .filename = "USBSCREEN  ",
    .attribute = {.volume_id = 1},
    .creation_time = 0,
    .creation_date = 0,
    .last_access_date = 0,
    .reserved_fat32 = {0},
    .last_write_time = 0,
    .last_write_date = 0,
    .first_logical_cluster = 0,
    .file_size = 0
};

int fat12_format(disk_t* disk)
{
    if(!disk)
        return -1;
    /** Sector 0. Boot sector */
    memcpy(disk->mem, &default_boot_sector, sizeof(fat_boot_sector_t));
    /** Sector 1. FAT */
    memset(disk->mem + DISK_BLOCK_SIZE, 0, DISK_BLOCK_SIZE);
    memcpy(disk->mem + DISK_BLOCK_SIZE, default_fat_entry, sizeof(default_fat_entry));
    /** Sector 2. Root directory */
    memset(disk->mem + DISK_BLOCK_SIZE * 2, 0, DISK_BLOCK_SIZE);
    memcpy(disk->mem + DISK_BLOCK_SIZE * 2, &volume_label_entry, sizeof(fat_directory_entry_t));
    return 0;
}

int fat12_open_next_file(disk_t* disk, fat12_file_reader_t* reader)
{
    if(!reader || !disk)
        return -1;
    fat_directory_entry_t* entry = NULL;
    for(;;)
    {
        reader->entry++;
        if(reader->entry >= BOOT_ENTRY_NUM)
            return -1;
        /** This is safe to do as disk->mem is aligned */
        entry = (fat_directory_entry_t*)(disk->mem + DISK_BLOCK_SIZE * 2 + reader->entry * sizeof(fat_directory_entry_t));
        if(entry->filename[0] == 0 || entry->filename[0] == 0xE5 || entry->filename[0] == 0x05 || entry->filename[0] == 0x2E)
            continue;
        if(entry->attribute.directory || entry->attribute.volume_id)
            continue;
        break;
    }
    memcpy(reader->filename, entry->filename, 11);
    reader->filename[11] = 0;
    reader->size = entry->file_size;
    reader->size_read = 0;
    reader->current_sector = entry->first_logical_cluster;
    return 0;
}

static uint16_t read_fat_entry_at(const uint8_t* fat, uint16_t index)
{
    if(index % 2 == 0)
    {
        return ((((uint16_t)(fat[index * 3 / 2 + 1])) << 8) | 
            ((uint16_t)(fat[index * 3 / 2]))) & 0xFFF;
    }
    else
    {
        return ((((uint16_t)(fat[index * 3 / 2])) >> 4) | 
            (((uint16_t)(fat[index * 3 / 2 + 1]))) << 4) & 0xFFF;
    }
}

const uint8_t* fat12_read_file_next_sector(disk_t* disk, fat12_file_reader_t* reader, int* size)
{
    if(!reader || !disk || !size)
        return NULL;
    if(reader->current_sector == 0 || reader->current_sector == 0xFFF || reader->size == 0)
        return NULL;
    if(reader->size_read >= reader->size)
        return NULL;
    int read_size = reader->size - reader->size_read < DISK_BLOCK_SIZE ? reader->size - reader->size_read : DISK_BLOCK_SIZE;
    const uint8_t* sector = disk->mem + DISK_BLOCK_SIZE * (1 + reader->current_sector);
    reader->size_read += read_size;

    reader->current_sector = read_fat_entry_at(disk->mem + DISK_BLOCK_SIZE, reader->current_sector);

    *size = read_size;
    return sector;
}


