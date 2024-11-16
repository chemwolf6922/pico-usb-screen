#pragma once

#include <stdint.h>

#define DISK_BLOCK_NUM 64
#define DISK_BLOCK_SIZE 512

typedef struct
{
    uint8_t mem[DISK_BLOCK_NUM * DISK_BLOCK_SIZE] __attribute__((aligned(4)));
    struct
    {
        /** DO NOT put a lot of logic in this! */
        void (*on_write)(void* ctx);
        void* on_write_ctx;
    } callbacks;
    struct
    {
        /** 
         * The disk itself only locks on write.
         * Since the read may across multiple blocks,
         * you should manage the read lock by yourself.
         */
        void (*rwlock_wrlock)(void* ctx);
        void (*rwlock_unlock)(void* ctx);
        void* rwlock_ctx;
    } hooks;
} disk_t;

/**
 * @brief This will not clean the disk content.
 * 
 * @param disk 
 * @return int 
 */
int disk_init(disk_t* disk);

int disk_write(disk_t* disk, uint32_t block, uint32_t offset, void const* src, uint32_t size);
int disk_read(disk_t* disk, uint32_t block, uint32_t offset, void* dst, uint32_t size);
