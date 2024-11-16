#pragma once

#include <stdint.h>
#include <FreeRTOS.h>
#include <timers.h>

typedef struct
{
    int pin;
    struct
    {
        void(*on_click)(void* ctx);
        void* on_click_ctx;
    } callback;
    struct
    {
        uint64_t last_trigger_us;
        TimerHandle_t debounce_timer;
    } internal;
} button_t;

int button_init(button_t* button);

void button_deinit(button_t* button);
