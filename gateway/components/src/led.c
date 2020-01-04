#include "led.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "wifi_manager.h"

// Indicate that the ESP32 is currently connected 
static const int LED_WIFI_CONNECTED_BIT = BIT0;

// Indicate that the ESP32 is currently connecting
static const int LED_WIFI_CONNECTING_BIT = BIT1;

void init_led(void)
{
	// Init led
	gpio_pad_select_gpio(LED_PIN);
	gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(LED_PIN, 0);
}

void led_task(void)
{
	init_led();

	led_event_group = xEventGroupCreate();

	EventBits_t uxBits;
	uint8_t led_status = 0;
	for(;;) {
		uxBits = xEventGroupWaitBits(led_event_group, LED_WIFI_CONNECTED_BIT | LED_WIFI_CONNECTING_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
		
		if(uxBits & BIT0) {
			gpio_set_level(LED_PIN, 1);
		}
		
		if(uxBits & BIT1) {
			gpio_set_level(LED_PIN, led_status);
			led_status = !led_status;
		}

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}