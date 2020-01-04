#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt.h"
#include "configuration.h"
#include "wifi_manager.h"
#include "driver/uart.h"
#include "cJSON.h"
#include "driver/uart.h"


static const char *MODULE_NAME = "[MQTT]";


esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char topic[30];
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, CONFIG_CHANNEL,  0);
            esp_mqtt_client_subscribe(client, CONTROL_CHANNEL, 0);
            mqtt_connected_flag = 1;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected_flag = 0;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_DATA");
            
            //printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            //printf("DATA=%.*s\r\n", event->data_len, event->data);
            memset(topic, '\0', sizeof(topic));
            strncpy(topic, event->topic, event->topic_len);
            //printf("%s \n", topic);
            
            char data[200];
            memset(data, '\0', sizeof(data));
            strncpy(data, event->data, event->data_len);
            strcat(data, "\n");

            printf("MQTT_Package: %s\n", data);

			if(xSemaphoreLora != NULL ) {
				if(xSemaphoreTake(xSemaphoreLora, (TickType_t) 10) == pdTRUE ) {
					if(strcmp(topic, CONFIG_CHANNEL) == 0 || strcmp(topic, CONTROL_CHANNEL) == 0) {
						uart_write_bytes(UART_NUM_1, (const char*)data, event->data_len + 1);   
					}
                    xSemaphoreGive(xSemaphoreLora);
				}
			}				

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MODULE_NAME, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(MODULE_NAME, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

void init_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri            = BROKER_URL,
        .port           = BROKER_PORT,
        .username       = BROKER_USERNAME,
        .password       = BROKER_PASSWORD,
        .event_handle   = mqtt_event_handler
    };

    while(1) {
        if(wifi_connected_flag & !mqtt_connected_flag) {
            mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
            esp_mqtt_client_start(mqtt_client);
        } 
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}