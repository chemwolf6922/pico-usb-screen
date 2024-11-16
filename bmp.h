#pragma once

#include <stdint.h>

/** Only supports uncompressed 8-8-8 pixel format */

typedef struct
{
    /** This is not the frame buffer size as there are paddings. */
    int pixel_array_size;
    int pixel_array_read;
    uint32_t width;
    uint32_t height;
} bmp_t;

/**
 * @brief Open a bmp file with partial data.
 * 
 * @param bmp 
 * @param data 
 * @param size 
 * @return int 
 */
int bmp_open(bmp_t* bmp, const uint8_t* data, uint32_t size);

/**
 * @brief 
 * 
 * @param bmp 
 * @param src 
 * @param src_size 
 * @param dst The whole frame buffer. DO NOT offset.
 * @param dst_size
 * @return int 
 */
int bmp_read_next(bmp_t* bmp, const uint8_t* src, uint32_t src_size, uint8_t* dst, uint32_t dst_size);
