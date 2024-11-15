#include "lcd.h"
#include <hardware/gpio.h>

/** @todo implement the dma path */

static void loop_sleep(uint32_t ms, void* ctx);
static int send_command(lcd_t* lcd, uint8_t* command_and_data, int size);
static int init_lcd_hardware(lcd_t* lcd);


int lcd_init(lcd_t* lcd)
{
    /** check parameters */
    if(!lcd)
        return -1;
    if(!lcd->spi)
        return -1;
    /** no checks for the pins. As that is overlay complected */
    /** check hook combinations */
    if(lcd->hooks.enter_critical_section || lcd->hooks.exit_critical_section)
    {
        if(!(lcd->hooks.enter_critical_section && lcd->hooks.exit_critical_section))
            return -1;
    }
    if(lcd->options.enable_dma)
    {
        if(!(lcd->hooks.semaphore_take && lcd->hooks.semaphore_give))
            return -1;
    }
    if(!lcd->hooks.sleep)
    {
        lcd->hooks.sleep = loop_sleep;
    }

    /** init peripherals */
    gpio_set_function(lcd->pin_clk, GPIO_FUNC_SPI);
    gpio_set_function(lcd->pin_miso, GPIO_FUNC_SPI);
    spi_init(lcd->spi, lcd->options.spi_baudrate);
    gpio_init(lcd->pin_ncs);
    gpio_set_dir(lcd->pin_ncs, GPIO_OUT);
    gpio_put(lcd->pin_ncs, 1);
    gpio_init(lcd->pin_nrst);
    gpio_set_dir(lcd->pin_nrst, GPIO_OUT);
    gpio_put(lcd->pin_nrst, 1);
    gpio_init(lcd->pin_dc);
    gpio_set_dir(lcd->pin_dc, GPIO_OUT);
    gpio_put(lcd->pin_dc, 1);

    /** init screen */

    if(init_lcd_hardware(lcd) != 0)
    {
        lcd_deinit(lcd);
        return -1;
    }

    return 0;
}

void lcd_deinit(lcd_t* lcd)
{
    spi_deinit(lcd->spi);
    gpio_set_function(lcd->pin_clk, GPIO_FUNC_NULL);
    gpio_set_function(lcd->pin_miso, GPIO_FUNC_NULL);
    gpio_set_function(lcd->pin_ncs, GPIO_FUNC_NULL);
    gpio_set_function(lcd->pin_nrst, GPIO_FUNC_NULL);
    gpio_set_function(lcd->pin_dc, GPIO_FUNC_NULL);
}

int lcd_enter_sleep(lcd_t* lcd)
{
#define SEND_COMMAND_OR_RETURN(...)  \
    do \
    { \
        if(send_command(lcd, (uint8_t[]){__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__})) != 0) \
        { \
            return -1; \
        } \
    } while (0)

    /** Display off */
    SEND_COMMAND_OR_RETURN(0x28);
    /** Sleep in */
    SEND_COMMAND_OR_RETURN(0x10);
    /** It take 120ms to get into sleep in mode after command is issued. */
    lcd->hooks.sleep(120 + 10, lcd->hooks.sleep_ctx);
    
    return 0;

#undef SEND_COMMAND_OR_RETURN
}

int lcd_exit_sleep(lcd_t* lcd)
{
#define SEND_COMMAND_OR_RETURN(...)  \
    do \
    { \
        if(send_command(lcd, (uint8_t[]){__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__})) != 0) \
        { \
            return -1; \
        } \
    } while (0)

    /** Sleep out. */
    SEND_COMMAND_OR_RETURN(0x11);
    /** It is necessary to wait 120ms after sending sleep out command before sleep in command can be sent. */
    lcd->hooks.sleep(120 + 10, lcd->hooks.sleep_ctx);
    /** Display on. */
    SEND_COMMAND_OR_RETURN(0x29);

    return 0;

#undef SEND_COMMAND_OR_RETURN
}


/** @todo implement dma */
int lcd_write_frame(lcd_t* lcd, const uint8_t frame[LCD_FRAME_SIZE])
{
    if(!lcd || !frame)
        return -1;
    int rc = 0;
    if(lcd->hooks.enter_critical_section)
        lcd->hooks.enter_critical_section(lcd->hooks.critical_section_ctx);

    gpio_put(lcd->pin_dc, 0);
    gpio_put(lcd->pin_ncs, 0);
    if(spi_write_blocking(lcd->spi, (uint8_t[]){0x2C}, 1) != 1)
    {
        rc = -1;
        goto finish;
    }
    gpio_put(lcd->pin_dc, 1);
    if(spi_write_blocking(lcd->spi, frame, LCD_FRAME_SIZE) != LCD_FRAME_SIZE)
    {
        rc = -1;
        goto finish;
    }

finish:
    gpio_put(lcd->pin_ncs, 1);
    if(lcd->hooks.exit_critical_section)
        lcd->hooks.exit_critical_section(lcd->hooks.critical_section_ctx);

    return rc;
}

static void loop_sleep(uint32_t ms, void* ctx)
{
    sleep_ms(ms);
}

static int send_command(lcd_t* lcd, uint8_t* command_and_data, int size)
{
    if(!lcd || !command_and_data || size < 1)
        return -1;
    int rc = 0;

    if(lcd->hooks.enter_critical_section)
        lcd->hooks.enter_critical_section(lcd->hooks.critical_section_ctx);

    gpio_put(lcd->pin_dc, 0);
    gpio_put(lcd->pin_ncs, 0);

    if(spi_write_blocking(lcd->spi, command_and_data, 1) != 1)
    {
        rc = -1;
        goto finish;
    }

    if(size > 1)
    {
        gpio_put(lcd->pin_dc, 1);
        if(spi_write_blocking(lcd->spi, command_and_data+1, size-1) != size-1)
        {
            rc = -1;
            goto finish;
        }
    }

finish:
    gpio_put(lcd->pin_ncs, 1);
    
    if(lcd->hooks.exit_critical_section)
        lcd->hooks.exit_critical_section(lcd->hooks.critical_section_ctx);

    return rc;
}


static int init_lcd_hardware(lcd_t* lcd)
{
#define SEND_COMMAND_OR_RETURN(...)  \
    do \
    { \
        if(send_command(lcd, (uint8_t[]){__VA_ARGS__}, sizeof((uint8_t[]){__VA_ARGS__})) != 0) \
        { \
            return -1; \
        } \
    } while (0)

    /** reset */
    lcd->hooks.sleep(50, lcd->hooks.sleep_ctx);
    gpio_put(lcd->pin_nrst, 0);
    lcd->hooks.sleep(50, lcd->hooks.sleep_ctx);
    gpio_put(lcd->pin_nrst, 1);
    lcd->hooks.sleep(120, lcd->hooks.sleep_ctx);

    /** Enable inter_command */
    SEND_COMMAND_OR_RETURN(0xFE);
    SEND_COMMAND_OR_RETURN(0xEF);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x86, 0xFF);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x87, 0xFF);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8E, 0xFF);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8F, 0xFF);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x80, 0x13);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x81, 0x40);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x82, 0x0A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x83, 0x0B);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x84, 0x60);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x85, 0x80);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x89, 0x10);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8A, 0x0F);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8B, 0x02);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8C, 0x59);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x8D, 0x55);
    /** Set pixel format, 18bit 6-6-6, actual data: 8-8-8 */
    SEND_COMMAND_OR_RETURN(0x3A, 0x66);
    /** Inversion, 1 dot */
    SEND_COMMAND_OR_RETURN(0xEC, 0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x7E, 0x30);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x74, 0x05, 0x4D, 0x00, 0x00, 0x01, 0x00, 0x00);
    /** Blanking porch control. With one parameter missing */
    SEND_COMMAND_OR_RETURN(0xB5, 0x0D, 0x0D);
    /** Display function control, source 1->240, gate 1->160 */
    SEND_COMMAND_OR_RETURN(0xB6, 0x00, 0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x60, 0x38, 0x09, 0x1E, 0x7A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x63, 0x38, 0xAE, 0x1E, 0x7A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x64, 0x38, 0x0B, 0x70, 0xAB, 0x1E, 0x7A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x66, 0x38, 0x0F, 0x70, 0xAF, 0x1E, 0x7A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x68, 0x00, 0x08, 0x07, 0x00, 0x07, 0x55, 0x6A);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x6A, 0x00, 0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x6C, 0x22, 0x02, 0x22, 0x02, 0x22, 0x22, 0x50);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x6E, 0x00, 0x00, 0x00, 0x02, 0x14, 0x12, 0x0C, 
                           0x0A, 0x1E, 0x1D, 0x08, 0x00, 0x16, 0x15, 0x00,
                           0x00, 0x00, 0x00, 0x15, 0x16, 0x00, 0x07, 0x1D,
                           0x1E, 0x09, 0x0B, 0x11, 0x13, 0x01, 0x00, 0x00,
                           0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x98, 0x3E);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x99, 0x3E);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x9B, 0x3B);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x93, 0x33, 0x7F, 0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x91, 0x0E, 0x09);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x70, 0x04, 0x02, 0x0D, 0x04, 0x02, 0x0D);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0x71, 0x04, 0x02, 0x0D);
    /** Power control 2 */
    SEND_COMMAND_OR_RETURN(0xC3, 0x26);
    /** Power control 3 */
    SEND_COMMAND_OR_RETURN(0xC4, 0x26);
    /** Power control 4 */
    SEND_COMMAND_OR_RETURN(0xC9, 0x1C);
    /** Set gamma 1 */
    SEND_COMMAND_OR_RETURN(0xF0, 0x02, 0x03, 0x0A, 0x06, 0x00, 0x1A);
    /** Set gamma 2 */
    SEND_COMMAND_OR_RETURN(0xF1, 0x38, 0x78, 0x1B, 0x2E, 0x2F, 0xC8);
    /** Set gamma 3 */
    SEND_COMMAND_OR_RETURN(0xF2, 0x02, 0x03, 0x0A, 0x06, 0x00, 0x1A);
    /** Set gamma 4 */
    SEND_COMMAND_OR_RETURN(0xF3, 0x38, 0x74, 0x12, 0x2E, 0x2F, 0xDF);
    /** Dual single gate select, single gate mode */
    SEND_COMMAND_OR_RETURN(0xBF, 0x00);
    /** Unknown command */
    SEND_COMMAND_OR_RETURN(0xF9, 0x40);
    /** Memory access control, all normal, RGB pixel @todo change to BGR */
    SEND_COMMAND_OR_RETURN(0x36, 0x00);
    /** Column address set, 15 -> 64 */
    SEND_COMMAND_OR_RETURN(0x2A, 0x00, 0x0F, 0x00, 0x40);
    /** Page address set, 0 -> 159 */
    SEND_COMMAND_OR_RETURN(0x2B, 0x00, 0x00, 0x00, 0x9F);
    /** This IS necessary after reset. */
    if(lcd_exit_sleep(lcd) != 0)
        return -1;

    return 0;

#undef SEND_COMMAND_OR_RETURN
}
