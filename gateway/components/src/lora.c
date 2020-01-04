#include "driver/uart.h"
#include "lora.h"
#include "mqtt.h"
#include "configuration.h"
#include "heart_beat.h"
#include "cJSON.h"

uint8_t init_lora_flag = 0;
unsigned int current_node = 1;
uint8_t count = 0;

void publish_data(uint8_t *message, int len) {

    if(mqtt_connected_flag) {
        if(message[0] != '\0') {
            printf("Published Message:\n\t%s\n", message);
            
            cJSON *json_message = cJSON_Parse((char *)message);

            cJSON *json_device_id   = cJSON_GetObjectItemCaseSensitive(json_message, "device_id");
            if (json_device_id == NULL) {
                goto end;
            }

            cJSON *json_temperature = cJSON_GetObjectItemCaseSensitive(json_message, "temperature");
            if (json_temperature == NULL) {
                goto end;
            }

            cJSON *json_humidity    = cJSON_GetObjectItemCaseSensitive(json_message, "humidity");
            if (json_humidity == NULL) {
                goto end;
            }

            cJSON *json_battery     = cJSON_GetObjectItemCaseSensitive(json_message, "battery");
            if (json_battery == NULL) {
                goto end;
            }

            printf("\tDevice ID: \t%d \n", json_device_id->valueint);
            printf("\tTemperature: \t%.2lf\n", json_temperature->valuedouble);
            printf("\tHumidity: \t%.2lf\n", json_humidity->valuedouble);
            printf("\tBattery: \t%d\n\n", json_battery->valueint);

            esp_mqtt_client_publish(mqtt_client, DEVICE_CHANNEL, (char *)message, len, 0, 0);

            // Track received from node
            completed_sending_a[json_device_id->valueint] = 1;
            
            if(current_node < NODE_NUM) {
                current_node++;  
            } else {
                current_node = 1;
            }
            
            count = 0;

            end:
                cJSON_Delete(json_message);

        }
    }       
}

void init_lora(void)
{
	// Create Binary Semaphore To Protect Lora Device
	vSemaphoreCreateBinary(xSemaphoreLora);
	
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_1, BUFF_SIZE * 2, 0, 0, NULL, 0);

    // Loop Function For Lora Device
    loop_lora();
}


void loop_lora(void)
{
    uint8_t *message = (uint8_t *) malloc(BUFF_SIZE);
    int i;
    init_lora_flag = 1;
    while(1) {
        // Check MQTT Status
        if(mqtt_connected_flag) {
            // Clear buffer 
            for(i=0; i<BUFF_SIZE; i++)
                message[i] = 0;
            // Read data from node
            int len = uart_read_bytes(UART_NUM_1, message, BUFF_SIZE, 1000 / portTICK_RATE_MS);
            if (len != 0 && message[0] == '{' && message[len-2] == '\r' && message[len-1] == '\n' ) {
                // printf("[Gateway] Data received = %s \n", message);
                // printf("[Gateway] len = %d \n", len);
                publish_data(message, len);
            }
        }
    }
}



/* Send signal to node to notify gateway can receive node's data */
void collision_solving_task(void) 
{   
    char *sending_packet = NULL;
    // Prepare sending packet
    cJSON *jsonObject = cJSON_CreateObject();
    cJSON_AddNumberToObject(jsonObject, "device_id", 0);
    cJSON_AddNumberToObject(jsonObject, "type", 3);
    while(1) {
        // Check Lora Device Available
		if(xSemaphoreLora != NULL ) {
			if(xSemaphoreTake(xSemaphoreLora, (TickType_t) 10) == pdTRUE ) {
                // Check Lora Status
				if(init_lora_flag) {
                    // Loop For Each Node Device
					for(int i=1; i <= NODE_NUM; i++) {
						if(completed_sending_a[i] == 0 && current_node == i) {
                            // Create sending package
							cJSON_SetIntValue(cJSON_GetObjectItem(jsonObject, "device_id"), i);
							sending_packet = cJSON_PrintUnformatted(jsonObject);
							strcat(sending_packet, "\r\n");
							
                            // Get package size
							int packet_size = 0;
							while(1) {
								if(sending_packet[packet_size] == '\n') {
									break;
								}
								packet_size++;
							}
							packet_size++;

                            // Send sending signal to lora node
							printf("[Gateway] Sending Packet: %s", sending_packet);
							uart_write_bytes(UART_NUM_1, (const char*)sending_packet, packet_size);
							count++;
							
                            // If gateway can't receive data from current node 3 times, reset counter and go to next node
							if(count == 3) {
								count = 0;
								current_node++;
                                if(current_node > NODE_NUM)
                                    current_node = 1;
							}
							vTaskDelay(5000 / portTICK_PERIOD_MS);
						}
					}
                    // Permit send sengding signal to node after loop all nodes (reset completed_sending_a variable)
                    for(int j=1; j<=NODE_NUM; j++)
                        completed_sending_a[j] = 0;
				}
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
            xSemaphoreGive(xSemaphoreLora);
		}
    }
}



