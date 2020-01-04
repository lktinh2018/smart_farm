#ifndef LED_H
#define LED_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define RED_LED_PIN         "HELLO"
#define GREEN_LED_PIN       "NONE"
#define BLUE_LED_PIN        "NONE"

#define LED_PIN 2

void init_led(void);

void led_task(void);

EventGroupHandle_t led_event_group;


#endif