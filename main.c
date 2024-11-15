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

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

static void test_screen();

static void usb_device_task(void *param);

int main()
{

    stdio_init_all();

    // board_init();

    // xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

    // vTaskStartScheduler();

    test_screen();

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

#define SCREEN_SPI_PORT spi0
#define SCREEN_CLK (2)
#define SCREEN_MISO (3)
#define SCREEN_NCS (5)
#define SCREEN_DC (6)
#define SCREEN_NRST (7)

static void screen_write(bool is_command, uint8_t data)
{
    gpio_put(SCREEN_DC, is_command ? 0 : 1);
    gpio_put(SCREEN_NCS, 0);
    spi_write_blocking(SCREEN_SPI_PORT, &data, 1);
    gpio_put(SCREEN_NCS, 1);
}

#define SCREEN_WRITE_COMMAND(data) screen_write(true, (data))
#define SCREEN_WRITE_DATA(data) screen_write(false, (data))

static void test_screen()
{
    /** init hardware */
    gpio_set_function(SCREEN_CLK, GPIO_FUNC_SPI);
    gpio_set_function(SCREEN_MISO, GPIO_FUNC_SPI);
    spi_init(SCREEN_SPI_PORT, 1000 * 1000);
    gpio_init(SCREEN_NCS);
    gpio_set_dir(SCREEN_NCS, GPIO_OUT);
    gpio_put(SCREEN_NCS, 1);
    gpio_init(SCREEN_NRST);
    gpio_set_dir(SCREEN_NRST, GPIO_OUT);
    gpio_put(SCREEN_NRST, 1);
    gpio_init(SCREEN_DC);
    gpio_set_dir(SCREEN_DC, GPIO_OUT);
    gpio_put(SCREEN_DC, 1);

    /** reset screen */
    sleep_ms(50);
    gpio_put(SCREEN_NRST, 0);
    sleep_ms(50);
    gpio_put(SCREEN_NRST, 1);
    sleep_ms(120);
    
    /** Enable inter_command */
    SCREEN_WRITE_COMMAND(0xFE);
	SCREEN_WRITE_COMMAND(0xEF);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x86);
	SCREEN_WRITE_DATA(0xFF);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x87);
	SCREEN_WRITE_DATA(0xFF);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8E);
	SCREEN_WRITE_DATA(0xFF);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8F);
	SCREEN_WRITE_DATA(0xFF);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x80);
	SCREEN_WRITE_DATA(0x13);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x81);
	SCREEN_WRITE_DATA(0x40);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x82);
	SCREEN_WRITE_DATA(0x0a);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x83);
	SCREEN_WRITE_DATA(0x0b);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x84);
	SCREEN_WRITE_DATA(0x60);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x85);
	SCREEN_WRITE_DATA(0x80);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x89);
	SCREEN_WRITE_DATA(0x10);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8A);
	SCREEN_WRITE_DATA(0x0F);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8B);
	SCREEN_WRITE_DATA(0x02);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8C);
	SCREEN_WRITE_DATA(0x59);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x8D);
	SCREEN_WRITE_DATA(0x55);

    /** Set pixel format */
	SCREEN_WRITE_COMMAND(0x3A);
    /** 18bit 6-6-6 */
	SCREEN_WRITE_DATA(0x66);

    /** Inversion */
	SCREEN_WRITE_COMMAND(0xEC);
    /** 1 dot for single gate, 1+2H1V for dual gate */
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x7E);
	SCREEN_WRITE_DATA(0x30);
	
    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x74);
	SCREEN_WRITE_DATA(0x05);
	SCREEN_WRITE_DATA(0x4d);
	
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x01);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);

    /** Blanking porch control. Does this matter for serial port? */
	SCREEN_WRITE_COMMAND(0xB5);
    /** The minimum value: 2,2,2 */
	SCREEN_WRITE_DATA(0x0D);
	SCREEN_WRITE_DATA(0x0D);
    // SCREEN_WRITE_DATA(0x02);

    /** Display function control */
	SCREEN_WRITE_COMMAND(0xB6);
	SCREEN_WRITE_DATA(0x00);
    /** source 1->240, gate 1->160 */
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x60);
	SCREEN_WRITE_DATA(0x38);
	SCREEN_WRITE_DATA(0x09);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x7A);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x63);
	SCREEN_WRITE_DATA(0x38);
	SCREEN_WRITE_DATA(0xAE);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x7A);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x64);
	SCREEN_WRITE_DATA(0x38);
	SCREEN_WRITE_DATA(0x0B);
	SCREEN_WRITE_DATA(0x70);
	SCREEN_WRITE_DATA(0xAB);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x7A);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x66);
	SCREEN_WRITE_DATA(0x38);
	SCREEN_WRITE_DATA(0x0F);
	SCREEN_WRITE_DATA(0x70);
	SCREEN_WRITE_DATA(0xAF);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x7A);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x68);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x08);
	SCREEN_WRITE_DATA(0x07);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x07);
	SCREEN_WRITE_DATA(0x55);
	SCREEN_WRITE_DATA(0x6A);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x6A);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x6C);
	SCREEN_WRITE_DATA(0x22);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x22);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x22);
	SCREEN_WRITE_DATA(0x22);
	SCREEN_WRITE_DATA(0x50);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x6E);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x14);
	SCREEN_WRITE_DATA(0x12);
	SCREEN_WRITE_DATA(0x0C);
	SCREEN_WRITE_DATA(0x0A);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x1D);
	SCREEN_WRITE_DATA(0x08);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x16);
	SCREEN_WRITE_DATA(0x15);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x15);
	SCREEN_WRITE_DATA(0x16);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x07);
	SCREEN_WRITE_DATA(0x1D);
	SCREEN_WRITE_DATA(0x1E);
	SCREEN_WRITE_DATA(0x09);
	SCREEN_WRITE_DATA(0x0B);
	SCREEN_WRITE_DATA(0x11);
	SCREEN_WRITE_DATA(0x13);
	SCREEN_WRITE_DATA(0x01);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x98);
	SCREEN_WRITE_DATA(0x3E);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x99);
	SCREEN_WRITE_DATA(0x3E);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x9b);
	SCREEN_WRITE_DATA(0x3b);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x93);
	SCREEN_WRITE_DATA(0x33);
	SCREEN_WRITE_DATA(0x7f);
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x91);
	SCREEN_WRITE_DATA(0x0E);
	SCREEN_WRITE_DATA(0x09);  

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x70);
	SCREEN_WRITE_DATA(0x04);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x0d);
	SCREEN_WRITE_DATA(0x04);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x0d);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0x71);
	SCREEN_WRITE_DATA(0x04);
	SCREEN_WRITE_DATA(0x02);
	SCREEN_WRITE_DATA(0x0d);

    /** Power control 2 */
	SCREEN_WRITE_COMMAND(0xc3);
	SCREEN_WRITE_DATA(0x26);

    /** Power control 3 */
	SCREEN_WRITE_COMMAND(0xc4);
	SCREEN_WRITE_DATA(0x26);
    
    /** Power control 4 */
    SCREEN_WRITE_COMMAND(0xc9);
	SCREEN_WRITE_DATA(0x1c);

    /** Set gamma 1 */
	SCREEN_WRITE_COMMAND(0xf0);
	SCREEN_WRITE_DATA(0x02);//V62[5:0]
	SCREEN_WRITE_DATA(0x03);//V61[5:0]
	SCREEN_WRITE_DATA(0x0a);//V59[4:0]
	SCREEN_WRITE_DATA(0x06);//V57[4:0]
	SCREEN_WRITE_DATA(0x00);//V63[7:4]/V50[3:0]
	SCREEN_WRITE_DATA(0x1a);//V43[6:0]

    /** Set gamma 2 */
	SCREEN_WRITE_COMMAND(0xf1);
	SCREEN_WRITE_DATA(0x38);//V20[6:0] 52
	SCREEN_WRITE_DATA(0x78);//V36[7:5]/V6[4:0]
	SCREEN_WRITE_DATA(0x1b);//V27[7:5]/V4[4:0]
	SCREEN_WRITE_DATA(0x2e);//V2[5:0]
	SCREEN_WRITE_DATA(0x2f);//V1[5:0]
	SCREEN_WRITE_DATA(0xc8);//V13[7:4]/V0[3:0]

    /** Set gamma 3 */
	SCREEN_WRITE_COMMAND(0xf2);
	SCREEN_WRITE_DATA(0x02);//V62[5:0]
	SCREEN_WRITE_DATA(0x03);//V61[5:0]
	SCREEN_WRITE_DATA(0x0a);//V59[4:0]
	SCREEN_WRITE_DATA(0x06);//V57[4:0]
	SCREEN_WRITE_DATA(0x00);//V63[7:4]/V50[3:0]
	SCREEN_WRITE_DATA(0x1a);//V43[6:0]

    /** Set gamma 4 */
	SCREEN_WRITE_COMMAND(0xf3);
	SCREEN_WRITE_DATA(0x38);//V20[6:0]  
	SCREEN_WRITE_DATA(0x74);//V36[7:5]/V6[4:0]
	SCREEN_WRITE_DATA(0x12);//V27[7:5]/V4[4:0]
	SCREEN_WRITE_DATA(0x2e);//V2[5:0]
	SCREEN_WRITE_DATA(0x2f);//V1[5:0]
	SCREEN_WRITE_DATA(0xdf);//V13[7:4]/V0[3:0]

    /** Dual single gate select */
	SCREEN_WRITE_COMMAND(0xBF);
    /** single gate mode */
	SCREEN_WRITE_DATA(0x00);

    /** Unknown command */
	SCREEN_WRITE_COMMAND(0xF9);
	SCREEN_WRITE_DATA(0x40);

    /** Memory access control */
	SCREEN_WRITE_COMMAND(0x36);
    /** all normal, RGB pixel */
	SCREEN_WRITE_DATA(0x00);

    /** Column address set */
	SCREEN_WRITE_COMMAND(0x2a);
    /** 0x000F, 15 */
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x0f);
    /** 0x0040, 64 */
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x40);

    /** Row address set */
	SCREEN_WRITE_COMMAND(0x2b);
    /** 0x0000, 0 */
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x00);
    /** 0x009F, 159 */
	SCREEN_WRITE_DATA(0x00);
	SCREEN_WRITE_DATA(0x9f);

    /** Sleep out mode. Is the default sleep mode? */
	SCREEN_WRITE_COMMAND(0x11);

    /** @todo Is this necessary? */
    sleep_ms(200); 

    /** Display on. Is the default off? */
	SCREEN_WRITE_COMMAND(0x29);

    /** write some white pixels to the screen */
    SCREEN_WRITE_COMMAND(0x2C);
    uint8_t white_pixel[] = {0xFF, 0xFF, 0xFF};
    uint8_t black_pixel[] = {0x00, 0x00, 0x00};
    gpio_put(SCREEN_DC, 1);
    gpio_put(SCREEN_NCS, 0);
    for(int i = 0; i < 160; i++)
    {
        for(int j = 0; j<25;j++)
        {
            spi_write_blocking(SCREEN_SPI_PORT, white_pixel, sizeof(white_pixel));
        }
        for(int j = 0; j<25;j++)
        {
            spi_write_blocking(SCREEN_SPI_PORT, black_pixel, sizeof(black_pixel));
        }
    }
    gpio_put(SCREEN_NCS, 1);
}
