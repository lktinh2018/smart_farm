#ifndef MQTT_H
#define MQTT_H

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


// #define BROKER_URL       "mqtt://192.168.1.254"
// #define BROKER_PORT      1883
// #define BROKER_USERNAME  "lktinh2018"
// #define BROKER_PASSWORD  "77778888@"

#define BROKER_URL       "mqtt://m16.cloudmqtt.com"
#define BROKER_PORT      11197
#define BROKER_USERNAME  "whkjteko"
#define BROKER_PASSWORD  "r5klW3fPMjcM"

        
#define CONFIG_CHANNEL    "/iFarm/config"
#define CONTROL_CHANNEL   "/iFarm/control"
#define DEVICE_CHANNEL    "/iFarm/device"

volatile esp_mqtt_client_handle_t mqtt_client;

void init_mqtt(void);

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);


#endif