#include "heart_beat.h"
#include "mqtt.h"
#include <time.h>
#include "configuration.h"

void check_heart_beat(void)
{
    unsigned long current_time;
    int i;
    char topic[100];
    char temp_str[10];

    for(i=0; i < SUPPORTED_NODE; i++) {
        last_time[i] = time(NULL);
    }
    while(1) {
        current_time = (unsigned long)time(NULL);
        for(i=0; i < SUPPORTED_NODE; i++) {
            if(current_time - last_time[i] > CHECK_TIME) {
                strcpy(topic, "/node/");
                itoa(i+1, temp_str, 10);
                strcat(topic, temp_str);
                strcat(topic, "/status");
                if(mqtt_connected_flag)
                    esp_mqtt_client_publish(mqtt_client, topic, "0", 1, 0, 0);
            }
        }
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

void reset_heart_beat(int device_id) {
    char topic[100];
    char temp_str[10];
    memset(topic, 0, 100 * sizeof(topic[0])); 
    strcat(topic, "/node/");
    itoa(device_id, temp_str, 10);
    strcat(topic, temp_str);
    strcat(topic, "/status");
    strcat(topic, "\0");
    if(mqtt_connected_flag)
        esp_mqtt_client_publish(mqtt_client, topic, "1", 1, 0, 0);
    last_time[device_id - 1] = time(NULL);
}