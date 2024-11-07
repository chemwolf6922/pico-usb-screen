#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

#include <bsp/board.h>
#include <tusb.h>
// This is included in tusb.h. Put here to make intellisense happy
#include "tusb_config.h"

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

static void usb_device_task(void *param);

int main()
{

    stdio_init_all();

    board_init();

    xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);

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