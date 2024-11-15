#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

#include <hardware/gpio.h>
#include <hardware/spi.h>

#include <bsp/board.h>
#include <tusb.h>
// This is included in tusb.h. Put here to make intellisense happy
#include "tusb_config.h"

#include "lcd.h"

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

static void usb_device_task(void *param);
static void lcd_task(void* param);

int main()
{

    stdio_init_all();

    // board_init();
    // xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 3, NULL);

	xTaskCreate(lcd_task, "lcd", 1024, NULL, configMAX_PRIORITIES - 3, NULL);

    vTaskStartScheduler();

    for (;;)
        tight_loop_contents();

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
    while (1)
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
	lcd.options.enable_dma = false;
	lcd.options.spi_baudrate = 20 * 1000 * 1000;
	lcd.hooks.enter_critical_section = lcd_enter_critical_section;
	lcd.hooks.exit_critical_section = lcd_exit_critical_section;
	lcd.hooks.sleep = lcd_sleep;
	lcd.hooks.semaphore_take = NULL;
	lcd.hooks.semaphore_give = NULL;
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



