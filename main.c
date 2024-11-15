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
#include "usb_drive.h"
#include "lcd.h"

static void usb_device_task(void *param);
static void lcd_task(void* param);

/** @todo Move to FAT */
static void disk_memory_init();

static lcd_t lcd = {0};
static uint8_t frame_buffer[LCD_FRAME_SIZE] = {0};
static disk_t disk = {0};

int main()
{

    stdio_init_all();

	disk.hooks.rwlock_rdlock = NULL;
	disk.hooks.rwlock_wrlock = NULL;
	disk.hooks.rwlock_unlock = NULL;
	disk.callbacks.on_write = NULL;
	disk_init(&disk);
	/** @todo move to FAT */
	disk_memory_init();

	/** Static init of the usb drive */
	char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2+1];
	pico_get_unique_board_id_string(board_id, sizeof(board_id));
	board_id[sizeof(board_id)-1] = '\0';
	usb_drive_init_singleton(board_id, &disk);

	/** Put the usb task to the lowest priority. This task is always busy. */
	/** @todo large stack size only for testing, shrink to 1024 */
    xTaskCreate(usb_device_task, "usbd", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);

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

/** @todo Move to FAT */

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

static void disk_memory_init()
{
	memset(disk.mem, 0, sizeof(disk.mem));
	for(int i = 0; i < sizeof(disk_init_values)/sizeof(disk_init_values[0]); i++)
	{
		memcpy(disk.mem + disk_init_values[i].offset, disk_init_values[i].data, disk_init_values[i].length);
	}
}
