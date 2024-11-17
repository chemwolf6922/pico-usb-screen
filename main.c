#include <pico/stdlib.h>
#include <pico/unique_id.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>

/** FreeRTOS */
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <timers.h>
#include <semphr.h>

/** TUSB */
#include <bsp/board.h>
#include <tusb.h>
// This is included in tusb.h. Put here to make intellisense happy
#include "tusb_config.h"

#include "disk.h"
#include "fat12.h"
#include "bmp.h"
#include "usb_drive.h"
#include "lcd.h"
#include "button.h"

/** This is ample time */
#define FILE_WRITE_FINISH_TIMEOUT_MS (30)
#define FILE_WRITE_FINISH_TIMEOUT_TICK (pdMS_TO_TICKS(FILE_WRITE_FINISH_TIMEOUT_MS))

static void disk_lock(void* );
static void disk_unlock(void* );
static void on_disk_write(uint32_t , void* );
static void disk_write_finish_timer_handler(TimerHandle_t timer);
static void usb_device_task(void *param);
static void lcd_task(void* param);
static int read_frame_buffer();
static void button_on_click(void* );

static lcd_t lcd = {0};
static uint8_t frame_buffer[LCD_FRAME_SIZE] = {0};
static disk_t disk = {0};
static button_t button = {0};

static TimerHandle_t disk_write_finish_timer = NULL;

enum
{
	LCD_COMMAND_NEW_FRAME,
	LCD_COMMAND_TOGGLE_SLEEP
};
typedef uint32_t lcd_command_t;

static QueueHandle_t lcd_command_queue = NULL;
static SemaphoreHandle_t disk_mutex = NULL;

int main()
{
    stdio_init_all();

	/** Init disk */
	disk.hooks.rwlock_wrlock = disk_lock;
	disk.hooks.rwlock_unlock = disk_unlock;
	disk.callbacks.on_write = on_disk_write;
	disk_init(&disk);
	fat12_format(&disk);

	/** Static init of the usb drive */
	char board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES*2+1];
	pico_get_unique_board_id_string(board_id, sizeof(board_id));
	board_id[sizeof(board_id)-1] = '\0';
	usb_drive_init_singleton(board_id, &disk);

	disk_mutex = xSemaphoreCreateMutex();
	disk_write_finish_timer = xTimerCreate("diskw", FILE_WRITE_FINISH_TIMEOUT_TICK, pdFALSE, NULL, disk_write_finish_timer_handler);
	lcd_command_queue = xQueueCreate(10, sizeof(lcd_command_t));
	/** Put the usb task to the lowest priority. This task is always busy. */
    xTaskCreate(usb_device_task, "usbd", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
	// The lcd task needs to be at a higher priority.
	xTaskCreate(lcd_task, "lcd", 1024, NULL, configMAX_PRIORITIES - 1, NULL);

	button.pin = 8;
	button.callback.on_click = button_on_click;
	button_init(&button);

    vTaskStartScheduler();

    for (;;)
	{
		tight_loop_contents();
	}

    return 0;
}

static void disk_lock(void* )
{
	xSemaphoreTake(disk_mutex, portMAX_DELAY);
}

static void disk_unlock(void* )
{
	xSemaphoreGive(disk_mutex);
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
static void usb_device_task(void* )
{
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

static void on_disk_write(uint32_t , void* )
{
	xTimerReset(disk_write_finish_timer, FILE_WRITE_FINISH_TIMEOUT_TICK);
}

static void disk_write_finish_timer_handler(TimerHandle_t )
{
	lcd_command_t command = LCD_COMMAND_NEW_FRAME;
	xQueueSend(lcd_command_queue, &command, 0);
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

	bool is_sleeping = false;
	bool new_frame_during_sleep = false;
	for(;;)
	{
		lcd_command_t command = 0;
		if(xQueueReceive(lcd_command_queue, &command, portMAX_DELAY) == pdTRUE)
		{
			switch(command)
			{
				case LCD_COMMAND_NEW_FRAME:
					if(is_sleeping)
					{
						new_frame_during_sleep = true;
					}
					else
					{
						if(read_frame_buffer() == 0)
						{
							lcd_write_frame(&lcd, frame_buffer);
						}
					}
					break;
				case LCD_COMMAND_TOGGLE_SLEEP:
					is_sleeping = !is_sleeping;
					if(is_sleeping)
					{
						lcd_enter_sleep(&lcd);
					}
					else
					{
						if(new_frame_during_sleep)
						{
							new_frame_during_sleep = false;
							if(read_frame_buffer() == 0)
							{
								lcd_write_frame(&lcd, frame_buffer);
							}
						}
						lcd_exit_sleep(&lcd);
					}
					break;
			}
		}
	}
}

static int read_frame_buffer()
{
	disk_lock(NULL);
	fat12_file_reader_t reader = {0};
	/** Only read the first file. */
	if(fat12_open_next_file(&disk, &reader) != 0)
		goto error;
	int sector_size = 0;
	bool bmp_opened = false;
	bmp_t bmp = {0};
	int total_read_size = 0;
	for(;;)
	{
		const uint8_t* sector = fat12_read_file_next_sector(&disk, &reader, &sector_size);
		if(sector == NULL)
			break;
		if(!bmp_opened)
		{
			if(bmp_open(&bmp, sector, sector_size)!=0)
				goto error;
			bmp_opened = true;
		}
		int read_size = bmp_read_next(
			&bmp,
			sector,
			sector_size,
			frame_buffer,
			sizeof(frame_buffer));
		if(read_size < 0)
			goto error;
		total_read_size += read_size;
	}
	/** Only read full frames */
	if(total_read_size != sizeof(frame_buffer))
		goto error;
	disk_unlock(NULL);
	return 0;
error:
	disk_unlock(NULL);
	return -1;
}

static void button_on_click(void* )
{
	lcd_command_t command = LCD_COMMAND_TOGGLE_SLEEP;
	xQueueSend(lcd_command_queue, &command, 0);
}
