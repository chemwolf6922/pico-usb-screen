#pragma once

/**
 * This is not a general gc9d01 library as there are too many undocumented commands in the configuration process.
 * This is only for the 1.12 inch 50*160 lcd with the gc9d01 driver.
 */

#include <pico/stdlib.h>
#include <hardware/spi.h>

#define LCD_WIDTH (50)
#define LCD_HEIGHT (160)
#define LCD_PIXEL_SIZE (3)
#define LCD_FRAME_SIZE (LCD_WIDTH*LCD_HEIGHT*LCD_PIXEL_SIZE)

typedef struct
{
    spi_inst_t* spi;
    int pin_clk;
    int pin_miso;
    int pin_ncs;
    int pin_dc;
    int pin_nrst;
    struct
    {
        /** For protection against interrupts during spi operations. Will ignore if set to NULL */
        void (*enter_critical_section)(void* ctx);
        void (*exit_critical_section)(void* ctx);
        void* critical_section_ctx;

        /** For working nicely with an operating system */
        /** Will use the default loop sleep if this is NULL */
        void (*sleep)(uint32_t ms, void* ctx);
        void* sleep_ctx;
        /** Required for dma. */
        void (*semaphore_take)(void* ctx);
        void (*semaphore_give)(void* ctx);
        void* semaphore_ctx;
    } hooks;
    struct
    {
        bool enable_dma;
        int spi_baudrate;
    } options;
} lcd_t;

/**
 * @brief This will init all the peripherals and init the lcd.
 * 
 * @param lcd 
 * @return int 
 */
int lcd_init(lcd_t* lcd);

void lcd_deinit(lcd_t* lcd);

/**
 * @brief This command has ~120ms sleep inside. 
 * This will also disable the display. Which sets the screen to black. The frame buffer is preserved.
 * 
 * 
 * @param lcd 
 * @return int 
 */
int lcd_enter_sleep(lcd_t* lcd);

int lcd_exit_sleep(lcd_t* lcd);

/**
 * @brief Write one frame to the LCD. 
 * Pixel format: BGR888
 * Left -> right, top -> bottom
 * 
 * @param lcd 
 * @param frame 
 * @return int 
 */
int lcd_write_frame(lcd_t* lcd, const uint8_t frame[LCD_FRAME_SIZE]);
