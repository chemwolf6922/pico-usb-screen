#pragma once

#include "disk.h"

int fat12_format(disk_t* disk);

typedef struct
{
    int entry;
    /** Convert trailing ' 's to '\0's */
    char filename[11 + 1];
    uint32_t size;
    uint32_t size_read;
    uint16_t current_sector;
} fat12_file_reader_t;

int fat12_open_next_file(disk_t* disk, fat12_file_reader_t* reader);

const uint8_t* fat12_read_file_next_sector(disk_t* disk, fat12_file_reader_t* reader, int* size);

