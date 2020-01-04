#include "led.h"
#include "wifi_manager.h"
#include "mqtt.h"
#include "lora.h"
//#include "heart_beat.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main()
{	
	// Initialize flash memory
	nvs_flash_init();

	xTaskCreate(&led_task				, "led_task"				, 2048, NULL, 3, NULL);

	// // Start the HTTP server task
	xTaskCreate(&http_server			, "http_server"				, 2048, NULL, 5, NULL);

	// // Start the wifi manager task
	xTaskCreate(&wifi_manager			, "wifi_manager"			, 4096, NULL, 4, NULL);
	
	xTaskCreate(&init_mqtt				, "init_mqtt"				, 2048, NULL, 2, NULL);

	xTaskCreate(&init_lora				, "init_lora"				, 2048, NULL, 1, NULL);

	xTaskCreate(&collision_solving_task	, "collision_solving_task"	, 2048, NULL, 4, NULL);

	/* Check heart beat every X second and publish into heart beat channel */
	//xTaskCreate(&check_heart_beat		, "check_heart_beat_task"	, 2048, NULL, 5, NULL);
}