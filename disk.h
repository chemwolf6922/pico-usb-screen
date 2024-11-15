#pragma once

#define DISK_STRING_DESCRIPTOR_SIZE 16

typedef struct
{
    char serial[DISK_STRING_DESCRIPTOR_SIZE + 1];
    uint32_t block_num;
    struct
    {
        int (*disk_write)(uint32_t block, uint32_t offset, void const* src, uint32_t size);
        int (*disk_read)(uint32_t block, uint32_t offset, void* dst, uint32_t size);
    } hooks;
} disk_t;

int disk_init(disk_t* disk);
