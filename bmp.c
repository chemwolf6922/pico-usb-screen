#include "bmp.h"
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct
{
    /** NOT null terminated */
    char type[2];
    uint32_t size;
    uint8_t reserved[4];
    uint32_t pixel_array_offset;
} __attribute__((packed)) bmp_file_header_t;

typedef struct
{
    uint32_t header_size;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t pixel_array_size;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t colors;
    uint32_t important_colors;
} __attribute__((packed)) bmp_basic_info_header_t;

int bmp_open(bmp_t* bmp, const uint8_t* data, uint32_t size)
{
    if(!bmp || !data || size < sizeof(bmp_file_header_t) + sizeof(bmp_basic_info_header_t))
        return -1;
    bmp_file_header_t* file_header = (bmp_file_header_t*)data;
    bmp_basic_info_header_t* basic_info_header = (bmp_basic_info_header_t*)(data + sizeof(bmp_file_header_t));
    if(file_header->type[0] != 'B' || file_header->type[1] != 'M')
        return -1;
    if(basic_info_header->header_size < sizeof(bmp_basic_info_header_t))
        return -1;
    if(basic_info_header->compression != 0)
        return -1;
    if(basic_info_header->bits_per_pixel != 24)
        return -1;
    memset(bmp, 0, sizeof(bmp_t));
    bmp->width = basic_info_header->width;
    bmp->height = basic_info_header->height;
    bmp->pixel_array_size = basic_info_header->pixel_array_size;
    bmp->pixel_array_read = 0;
    return 0;
}

int bmp_read_next(bmp_t* bmp, const uint8_t* src, uint32_t src_size, uint8_t* dst, uint32_t dst_size)
{
    /** Pixels are left->right, bottom->up. And each row padded to multiple of 4. */
    if(!bmp || !src || !dst)
        return -1;
    if(bmp->pixel_array_read >= bmp->pixel_array_size)
        return 0;
    int total_bytes_in_row = (bmp->width * 3 + 3) & ~3;
    int useful_bytes_in_row = bmp->width * 3;
    if(useful_bytes_in_row * bmp->height > dst_size)
        return -1;
    int pos = 0;
    int useful_bytes_read = 0;
    if(bmp->pixel_array_read == 0)
    {
        /** skip the head */
        if(src_size < sizeof(bmp_file_header_t))
            return -1;
        bmp_file_header_t* file_header = (bmp_file_header_t*)src;
        /** Add == to avoid handle the header twice. */
        if(file_header->pixel_array_offset >= src_size)
            return -1;
        pos = file_header->pixel_array_offset;
    }
    while(pos < src_size && bmp->pixel_array_read < bmp->pixel_array_size)
    {
        int row = bmp->height - 1 - bmp->pixel_array_read / total_bytes_in_row;
        if(row < 0)
            break;
        int bytes_read_in_row = bmp->pixel_array_read % total_bytes_in_row;
        if(bytes_read_in_row < useful_bytes_in_row)
        {
            int bytes_to_read = MIN(useful_bytes_in_row - bytes_read_in_row, src_size - pos);
            memcpy(dst + row * useful_bytes_in_row + bytes_read_in_row, src + pos, bytes_to_read);
            pos += bytes_to_read;
            useful_bytes_read += bytes_to_read;
            bmp->pixel_array_read += bytes_to_read;
        }
        else
        {
            int bytes_to_skip = MIN(total_bytes_in_row - bytes_read_in_row, src_size - pos);
            pos += bytes_to_skip;
            bmp->pixel_array_read += bytes_to_skip;
        }
    }
    return useful_bytes_read;
}


