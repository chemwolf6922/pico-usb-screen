#include <pico/stdlib.h>
#include <pico/unique_id.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>

/** FreeRTOS */
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

/** TUSB */
#include <bsp/board.h>
#include <tusb.h>
// This is included in tusb.h. Put here to make intellisense happy
#include "tusb_config.h"

#include "disk.h"
#include "disk_common.h"
#include "lcd.h"

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

static void usb_device_task(void *param);
static void lcd_task(void* param);


#define DISK_BLOCK_NUM 64
static void disk_memory_init();
static int disk_write(uint32_t block, uint32_t offset, void const* src, uint32_t size);
static int disk_read(uint32_t block, uint32_t offset, void* dest, uint32_t size);

disk_t disk = {0};

int main()
{

    stdio_init_all();

	disk_memory_init();

	char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2+1];
	pico_get_unique_board_id_string(board_id, sizeof(board_id));
	strncpy(disk.serial, board_id, sizeof(disk.serial));
	disk.block_num = DISK_BLOCK_NUM;
	disk.hooks.disk_read = disk_read;
	disk.hooks.disk_write = disk_write;
	
	disk_init(&disk);

	/** Put the usb task to the lowest priority. This task is always busy. */
    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

	// The lcd task needs to be at a higher priority.
	xTaskCreate(lcd_task, "lcd", 1024, NULL, configMAX_PRIORITIES - 1, NULL);

    vTaskStartScheduler();

    for (;;)
	{
		tight_loop_contents();
	}

    return 0;
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void *param)
{
    (void)param;

    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    // RTOS forever loop
    for(;;)
    {
        // put this thread to waiting state until there is new events
        tud_task();
    }
}

static lcd_t lcd;
static uint8_t frame_buffer[LCD_FRAME_SIZE] = {0};

static void lcd_enter_critical_section(void* )
{
	taskENTER_CRITICAL();
}

static void lcd_exit_critical_section(void* )
{
	taskEXIT_CRITICAL();
}

static void lcd_sleep(uint32_t ms, void* )
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

static void lcd_task(void* )
{
	memset(&lcd, 0, sizeof(lcd_t));
	lcd.spi = spi0;
	lcd.pin_clk = 2;
	lcd.pin_miso = 3;
	lcd.pin_ncs = 5;
	lcd.pin_dc = 6;
	lcd.pin_nrst = 7;
	lcd.options.spi_baudrate = 20 * 1000 * 1000;
	lcd.hooks.enter_critical_section = lcd_enter_critical_section;
	lcd.hooks.exit_critical_section = lcd_exit_critical_section;
	lcd.hooks.sleep = lcd_sleep;
	lcd_init(&lcd);
	for(;;)
	{
		for(int i = 0; i < sizeof(frame_buffer)/3; i++)
		{
			/** B */
			frame_buffer[i*3] = 0x00;
			/** G */
			frame_buffer[i*3+1] = 0x00;
			/** R */
			frame_buffer[i*3+2] = 0xFF;
		}
		lcd_write_frame(&lcd, frame_buffer);	
		vTaskDelay(pdMS_TO_TICKS(1000));
		for(int i = 0; i < sizeof(frame_buffer)/3; i++)
		{
			/** B */
			frame_buffer[i*3] = 0x00;
			/** G */
			frame_buffer[i*3+1] = 0xFF;
			/** R */
			frame_buffer[i*3+2] = 0x00;
		}
		lcd_write_frame(&lcd, frame_buffer);	
		vTaskDelay(pdMS_TO_TICKS(1000));
		/** Test refreshing in sleep */
		lcd_enter_sleep(&lcd);
		for(int i = 0; i < sizeof(frame_buffer)/3; i++)
		{
			/** B */
			frame_buffer[i*3] = 0xFF;
			/** G */
			frame_buffer[i*3+1] = 0x00;
			/** R */
			frame_buffer[i*3+2] = 0x00;
		}
		lcd_write_frame(&lcd, frame_buffer);
		lcd_exit_sleep(&lcd);	
		vTaskDelay(pdMS_TO_TICKS(1000));
	}	
}


#define ARRAY_SIZE_AND_CONTENT(...) (sizeof((uint8_t[]){__VA_ARGS__})),(uint8_t[]){__VA_ARGS__}
const struct
{
	int offset;
	int length;
	uint8_t* data;
} disk_init_values[] = {
	/** Boot sector */
	{0, ARRAY_SIZE_AND_CONTENT(
		0xEB, 0x3C, 0x90, 
        0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 
        0x00, 0x02, 
        0x01, 
        0x01, 0x00,
        0x01, 
        0x10, 0x00, 
        0x40, 0x00, 
        0xF8, 
        0x01, 0x00, 
        0x01, 0x00, 
        0x01, 0x00, 
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 
        0x80, 
        0x00, 
        0x29, 
        0x34, 0x12, 0x00, 0x00,  
         'U',  'S',  'B',  'S',  'C',  'R',  'E',  'E',  'N',  ' ',  ' ', 
        0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 
	)},
	/** Magic */
	{510, ARRAY_SIZE_AND_CONTENT(
		0x55, 0xAA
	)},
	/** FAT12 table */
	{512, ARRAY_SIZE_AND_CONTENT(
		0xF8, 0xFF, 0xFF,
	)},
	/** Volume label */
	{1024, ARRAY_SIZE_AND_CONTENT(
        'U', 'S', 'B', 'S', 'C', 'R', 'E', 'E', 'N', ' ', ' ', 0x08, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	)}
};

static uint8_t disk_memory[DISK_BLOCK_NUM*DISK_BLOCK_SIZE] = {0};

static void disk_memory_init()
{
	memset(disk_memory, 0, sizeof(disk_memory));
	for(int i = 0; i < sizeof(disk_init_values)/sizeof(disk_init_values[0]); i++)
	{
		memcpy(disk_memory + disk_init_values[i].offset, disk_init_values[i].data, disk_init_values[i].length);
	}
}

static int disk_write(uint32_t block, uint32_t offset, void const* src, uint32_t size)
{
	if(block * DISK_BLOCK_SIZE + offset + size > sizeof(disk_memory))
	{
		return -1;
	}
	if(offset + size > DISK_BLOCK_SIZE)
	{
		return -1;
	}
	memcpy(disk_memory + block * DISK_BLOCK_SIZE + offset, src, size);
	return size;
}

static int disk_read(uint32_t block, uint32_t offset, void* dest, uint32_t size)
{
	if(block * DISK_BLOCK_SIZE + offset + size > sizeof(disk_memory))
	{
		return -1;
	}
	if(offset + size > DISK_BLOCK_SIZE)
	{
		return -1;
	}
	memcpy(dest, disk_memory + block * DISK_BLOCK_SIZE + offset, size);
	return size;
}
