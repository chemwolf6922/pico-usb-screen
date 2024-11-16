#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include "button.h"

#define PICO_GPIO_NUM (29)
#define BUTTON_DEBOUNCE_US (100000)
#define BUTTON_DEBOUNCE_TICKS (pdMS_TO_TICKS(BUTTON_DEBOUNCE_US / 1000))

static button_t* buttons[PICO_GPIO_NUM] = {0};
static void button_irq_handler(uint gpio, uint32_t events);
static void button_timer_handler(TimerHandle_t timer);

int button_init(button_t* button)
{
    if(!button || !button->callback.on_click)
        return -1;
    if(button->pin < 0 || button->pin >= PICO_GPIO_NUM)
        return -1;
    if(buttons[button->pin])
        return -1;
    buttons[button->pin] = button;
    button->internal.debounce_timer = xTimerCreate(
        "budeb",
        BUTTON_DEBOUNCE_TICKS,
        pdFALSE,
        button,
        button_timer_handler);
    gpio_init(button->pin);
    gpio_set_dir(button->pin, GPIO_IN);
    gpio_pull_up(button->pin);
    gpio_set_irq_enabled_with_callback(button->pin, GPIO_IRQ_EDGE_FALL, true, button_irq_handler);
    return 0;
}

void button_deinit(button_t* button)
{
    if(!button)
        return;
    if(button->pin < 0 || button->pin >= PICO_GPIO_NUM)
        return;
    if(buttons[button->pin] != button)
        return;
    buttons[button->pin] = NULL;
    gpio_set_irq_enabled(button->pin, GPIO_IRQ_EDGE_FALL, false);
    xTimerDelete(button->internal.debounce_timer, 0);
    gpio_deinit(button->pin);
}

static void button_irq_handler(uint gpio, uint32_t events)
{
    if(!buttons[gpio])
        return;
    button_t* button = buttons[gpio];
    uint64_t then = button->internal.last_trigger_us;
    uint64_t now = to_us_since_boot(get_absolute_time());
    button->internal.last_trigger_us = now;
    if(now - then < BUTTON_DEBOUNCE_US)
        return;
    xTimerResetFromISR(button->internal.debounce_timer, NULL);
}

static void button_timer_handler(TimerHandle_t timer)
{
    button_t* button = pvTimerGetTimerID(timer);
    /** This should not happen */
    if(!buttons[button->pin])
        return;
    /** Pin should be low */
    if(gpio_get(button->pin))
        return;
    button->callback.on_click(button->callback.on_click_ctx);
}
