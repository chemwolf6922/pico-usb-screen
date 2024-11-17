#include <stdint.h>
#include <string.h>

#include "disk.h"

int disk_init(disk_t* disk)
{
    if(!disk)
        return -1;
    if(disk->hooks.rwlock_wrlock || disk->hooks.rwlock_unlock)
    {
        if(!disk->hooks.rwlock_wrlock || !disk->hooks.rwlock_unlock)
            return -1;
    }
    return 0;
}

int disk_write(disk_t* disk, uint32_t block, uint32_t offset, void const* src, uint32_t size)
{
	if(block * DISK_BLOCK_SIZE + offset + size > sizeof(disk->mem))
	{
		return -1;
	}
	if(offset + size > DISK_BLOCK_SIZE)
	{
		return -1;
	}
    if(disk->hooks.rwlock_wrlock)
        disk->hooks.rwlock_wrlock(disk->hooks.rwlock_ctx);

	memcpy(disk->mem + block * DISK_BLOCK_SIZE + offset, src, size);

    if(disk->hooks.rwlock_unlock)
        disk->hooks.rwlock_unlock(disk->hooks.rwlock_ctx);

    if(disk->callbacks.on_write)
        disk->callbacks.on_write(block, disk->callbacks.on_write_ctx);
	return size;
}

int disk_read(disk_t* disk, uint32_t block, uint32_t offset, void* dest, uint32_t size)
{
	if(block * DISK_BLOCK_SIZE + offset + size > sizeof(disk->mem))
	{
		return -1;
	}
	if(offset + size > DISK_BLOCK_SIZE)
	{
		return -1;
	}
	memcpy(dest, disk->mem + block * DISK_BLOCK_SIZE + offset, size);
	return size;
}
