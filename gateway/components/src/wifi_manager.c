#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "json.h"
#include "http_server.h"
#include "wifi_manager.h"
#include "configuration.h"
#include "led.h"


// Indicate that the ESP32 is currently connected 
static const int LED_WIFI_CONNECTED_BIT = BIT0;

// Indicate that the ESP32 is currently connecting
static const int LED_WIFI_CONNECTING_BIT = BIT1;

// Indicate that the ESP32 is currently connected 
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

// Set automatically once the SoftAP is started
const int WIFI_MANAGER_AP_STARTED = BIT2;

// When set, means a client requested to connect to an access point
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

// This bit is set automatically as soon as a connection was lost
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

// When set, means a client requested to scan wireless networks
const int WIFI_MANAGER_REQUEST_WIFI_SCAN = BIT5;

// When set, means a client requested to disconnect from currently connected AP.
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT = BIT6;

SemaphoreHandle_t wifi_manager_json_mutex = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records; //[MAX_AP_NUM];
char *accessp_json = NULL;
char *ip_info_json = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;


struct wifi_settings_t wifi_settings = {
	.ap_ssid		= AP_SSID,
	.ap_pwd			= AP_PASSWORD,
	.ap_channel		= AP_CHANNEL,
	.ap_ssid_hidden = AP_SSID_HIDDEN,
	.ap_bandwidth	= AP_BANDWIDTH,
	.sta_only		= STA_ONLY,
	.sta_power_save = STA_POWER_SAVE,
	.sta_static_ip 	= 0,
};

static const char *MODULE_NAME = "[WIFI_MANAGER]";


const char wifi_manager_nvs_namespace[] = "espwifimgr";


void wifi_manager_scan_async()
{
	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_SCAN);
}


void wifi_manager_disconnect_async()
{
	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_DISCONNECT);
}


esp_err_t wifi_manager_save_sta_config()
{
	nvs_handle handle;
	esp_err_t esp_err;

	if(wifi_manager_config_sta) {
		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK) return esp_err;

		esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
		if (esp_err != ESP_OK) return esp_err;

		esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
		if (esp_err != ESP_OK) return esp_err;

		esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
		if (esp_err != ESP_OK) return esp_err;

		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);
	}
	return ESP_OK;
}

bool wifi_manager_fetch_wifi_sta_config()
{
	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle) == ESP_OK){

		if(wifi_manager_config_sta == NULL) {
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
		memset(&wifi_settings, 0x00, sizeof(struct wifi_settings_t));

		// Allocate buffer
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		// Load SSID from flash memory
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK) {
			free(buff);
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		// Load PASSWORD from flash memory
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK) {
			free(buff);
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		// Load SETTING from flash memory
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

		free(buff);
		nvs_close(handle);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';
	} else {
		return false;
	}
}

void wifi_manager_clear_ip_info_json()
{
	strcpy(ip_info_json, "{}\n");
}

void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code)
{
	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	if(config){

		const char ip_info_json_format[] = ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n";

		memset(ip_info_json, 0x00, JSON_IP_INFO_SIZE);

		/* to avoid declaring a new buffer we copy the data directly into the buffer at its correct address */
		strcpy(ip_info_json, "{\"ssid\":");
		json_print_string(config->sta.ssid,  (unsigned char*)(ip_info_json+strlen(ip_info_json)) );

		if(update_reason_code == UPDATE_CONNECTION_OK){
			/* rest of the information is copied after the ssid */
			tcpip_adapter_ip_info_t ip_info;
			ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
			char ip[IP4ADDR_STRLEN_MAX]; /* note: IP4ADDR_STRLEN_MAX is defined in lwip */
			char gw[IP4ADDR_STRLEN_MAX];
			char netmask[IP4ADDR_STRLEN_MAX];
			strcpy(ip, ip4addr_ntoa(&ip_info.ip));
			strcpy(netmask, ip4addr_ntoa(&ip_info.netmask));
			strcpy(gw, ip4addr_ntoa(&ip_info.gw));

			snprintf( (ip_info_json + strlen(ip_info_json)), JSON_IP_INFO_SIZE, ip_info_json_format,
					ip,
					netmask,
					gw,
					(int)update_reason_code);
		}
		else {
			/* notify in the json output the reason code why this was updated without a connection */
			snprintf( (ip_info_json + strlen(ip_info_json)), JSON_IP_INFO_SIZE, ip_info_json_format,
								"0",
								"0",
								"0",
								(int)update_reason_code);
		}
	}
	else {
		wifi_manager_clear_ip_info_json();
	}
}

void wifi_manager_clear_access_points_json()
{
	strcpy(accessp_json, "[]\n");
}

void wifi_manager_generate_acess_points_json()
{

	strcpy(accessp_json, "[");

	const char oneap_str[] = ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n";

	/* stack buffer to hold on to one AP until it's copied over to accessp_json */
	char one_ap[JSON_ONE_APP_SIZE];
	for(int i=0; i<ap_num;i++){

		wifi_ap_record_t ap = accessp_records[i];

		/* ssid needs to be json escaped. To save on heap memory it's directly printed at the correct address */
		strcat(accessp_json, "{\"ssid\":");
		json_print_string( (unsigned char*)ap.ssid,  (unsigned char*)(accessp_json+strlen(accessp_json)) );

		/* print the rest of the json for this access point: no more string to escape */
		snprintf(one_ap, (size_t)JSON_ONE_APP_SIZE, oneap_str,
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?']':',');

		/* add it to the list */
		strcat(accessp_json, one_ap);
	}
}


bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait)
{
	if(wifi_manager_json_mutex){
		if( xSemaphoreTake( wifi_manager_json_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_json_buffer()
{
	xSemaphoreGive( wifi_manager_json_mutex );
}

char* wifi_manager_get_ap_list_json()
{
	return accessp_json;
}

esp_err_t wifi_manager_event_handler(void *ctx, system_event_t *event)
{
	
    switch(event->event_id) {

		case SYSTEM_EVENT_AP_START:
			printf("AP_START \n");
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED);
			break;

		case SYSTEM_EVENT_AP_STACONNECTED:
			printf("AP_STACONNECTED \n");
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
			break;

		case SYSTEM_EVENT_AP_STADISCONNECTED:
			printf("AP_STADISCONNECTED: \n");
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
			break;

		case SYSTEM_EVENT_STA_START:
			printf("STA_START: \n");
			break;

		case SYSTEM_EVENT_STA_GOT_IP:
		    ESP_LOGI(MODULE_NAME, "Connected !!! IP Address is %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
			xEventGroupClearBits(led_event_group, LED_WIFI_CONNECTING_BIT);
			xEventGroupSetBits(led_event_group, LED_WIFI_CONNECTED_BIT);
			wifi_connected_flag = 1;
			break;

		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGI(MODULE_NAME,"Disconnect !!! Retry to connect to the AP...");
			esp_wifi_connect();
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
			xEventGroupClearBits(led_event_group, LED_WIFI_CONNECTED_BIT);
			xEventGroupSetBits(led_event_group, LED_WIFI_CONNECTING_BIT);
			wifi_connected_flag = 0;
			break;

		default:
			break;
    }
	return ESP_OK;
}

wifi_config_t* wifi_manager_get_wifi_sta_config()
{
	return wifi_manager_config_sta;
}

void wifi_manager_connect_async()
{
	/* In order to avoid a false positive on the front end app we need to quickly flush the ip json
	 * There'se a risk the front end sees an IP or a password error when in fact
	 * it's a remnant from a previous connection
	 */
	if(wifi_manager_lock_json_buffer(portMAX_DELAY)) {
		wifi_manager_clear_ip_info_json();
		wifi_manager_unlock_json_buffer();
	}
	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
}

char* wifi_manager_get_ip_info_json()
{
	return ip_info_json;
}

void wifi_manager_destroy()
{
	// Heap buffer
	free(accessp_records);
	accessp_records = NULL;
	free(accessp_json);
	accessp_json = NULL;
	free(ip_info_json);
	ip_info_json = NULL;
	if(wifi_manager_config_sta) {
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);

	vTaskDelete(NULL);
}


void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps)
{
	int total_unique;
	wifi_ap_record_t * first_free;
	total_unique=*aps;

	first_free=NULL;

	for(int i=0; i<*aps-1;i++) {
		wifi_ap_record_t * ap = &aplist[i];

		/* skip the previously removed APs */
		if (ap->ssid[0] == 0) continue;

		/* remove the identical SSID+authmodes */
		for(int j=i+1; j<*aps;j++) {
			wifi_ap_record_t * ap1 = &aplist[j];
			if ( (strcmp((const char *)ap->ssid, (const char *)ap1->ssid)==0) && 
			     (ap->authmode == ap1->authmode) ) { /* same SSID, different auth mode is skipped */
				/* save the rssi for the display */
				if ((ap1->rssi) > (ap->rssi)) ap->rssi=ap1->rssi;
				/* clearing the record */
				memset(ap1,0, sizeof(wifi_ap_record_t));
			}
		}
	}
	/* reorder the list so APs follow each other in the list */
	for(int i=0; i<*aps;i++) {
		wifi_ap_record_t * ap = &aplist[i];
		/* skipping all that has no name */
		if (ap->ssid[0] == 0) {
			/* mark the first free slot */
			if (first_free==NULL) first_free=ap;
			total_unique--;
			continue;
		}
		if (first_free!=NULL) {
			memcpy(first_free, ap, sizeof(wifi_ap_record_t));
			memset(ap,0, sizeof(wifi_ap_record_t));
			/* find the next free slot */
			for(int j=0; j<*aps;j++) {
				if (aplist[j].ssid[0]==0) {
					first_free=&aplist[j];
					break;
				}
			}
		}
	}
	/* update the length of the list */
	*aps = total_unique;
}

void wifi_manager( void * pvParameters )
{
	// Memory allocation of objects used by the task
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(MAX_AP_NUM * JSON_ONE_APP_SIZE + 4); // 4 bytes for json encapsulation of "[\n" and "]\0" 
	wifi_manager_clear_access_points_json();
	ip_info_json = (char*)malloc(sizeof(char) * JSON_IP_INFO_SIZE);
	wifi_manager_clear_ip_info_json();
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
	memset(&wifi_settings.sta_static_ip_config, 0x00, sizeof(tcpip_adapter_ip_info_t));

	IP4_ADDR(&wifi_settings.sta_static_ip_config.ip, 192, 168, 0, 10);
	IP4_ADDR(&wifi_settings.sta_static_ip_config.gw, 192, 168, 0, 1);
	IP4_ADDR(&wifi_settings.sta_static_ip_config.netmask, 255, 255, 255, 0);

	// Initialize the TCP stack
	tcpip_adapter_init();

	// Event handler and event group for the wifi driver
	wifi_manager_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_manager_event_handler, NULL));

	// Wifi scanner config
	wifi_scan_config_t scan_config = {
		.ssid			= 0,
		.bssid			= 0,
		.channel		= 0,
		.show_hidden 	= true
	};

	// Try to get access to previously saved wifi 
	if(wifi_manager_fetch_wifi_sta_config()) {
		// Request a connection
		xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
	}

	// Start the softAP and stop DHCP server
	//ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

	// Assign a static IP to the AP network interface
	tcpip_adapter_ip_info_t info;
	memset(&info, 0x00, sizeof(info));
	//IP4_ADDR(&info.ip, 192, 168, 1, 38);
	//IP4_ADDR(&info.gw, 192, 168, 1, 1);
	//IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	//SP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));

	// Start DHCP server
	//ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

	tcpip_adapter_dhcp_status_t status;
	if(wifi_settings.sta_static_ip) {
#if WIFI_MANAGER_DEBUG
		printf("wifi_manager: assigning static ip to STA interface. IP: %s , GW: %s , Mask: %s\n", ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip), ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw), ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));
#endif
		/* stop DHCP client*/
		ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));

		/* assign a static IP to the STA network interface */
		ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &wifi_settings.sta_static_ip_config));
		}
	else {
		/* start DHCP client if not started*/
#if WIFI_MANAGER_DEBUG
		printf("wifi_manager: Start DHCP client for STA interface. If not already running\n");
#endif
		ESP_ERROR_CHECK(tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &status));
		if (status!=TCPIP_ADAPTER_DHCP_STARTED)
			ESP_ERROR_CHECK(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
	}


	// Init wifi as station + access point
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifi_init_config);
	esp_wifi_set_storage(WIFI_STORAGE_RAM);
	esp_wifi_set_mode(WIFI_MODE_APSTA);
	esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth);
	esp_wifi_set_ps(wifi_settings.sta_power_save);

	// Configure the softAP and start it
	wifi_config_t ap_config = {
		.ap = {
			.ssid_len			= 0,
			.channel			= wifi_settings.ap_channel,
			.authmode			= WIFI_AUTH_WPA2_PSK,
			.ssid_hidden		= wifi_settings.ap_ssid_hidden,
			.max_connection 	= AP_MAX_CONNECTIONS,
			.beacon_interval 	= AP_BEACON_INTERVAL,
		},
	};

	memcpy(wifi_settings.ap_ssid	, AP_SSID		, sizeof(AP_SSID));
	memcpy(wifi_settings.ap_pwd		, AP_PASSWORD	, sizeof(AP_PASSWORD));

	memcpy(ap_config.ap.ssid	, wifi_settings.ap_ssid , sizeof(wifi_settings.ap_ssid));
	memcpy(ap_config.ap.password, wifi_settings.ap_pwd	, sizeof(wifi_settings.ap_pwd));

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());


#if WIFI_MANAGER_DEBUG
	printf("wifi_manager: starting softAP with ssid %s\n", ap_config.ap.ssid);
	if(wifi_settings.ap_bandwidth == 1)
	printf("wifi_manager: starting softAP with 20 MHz bandwidth\n");
	else printf("wifi_manager: starting softAP with 40 MHz bandwidth\n");
	printf("wifi_manager: starting softAP on channel %i\n", wifi_settings.ap_channel);
	if(wifi_settings.sta_power_save ==1) printf("wifi_manager: STA power save enabled\n");
#endif

	/* wait for access point to start */
	xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED, pdFALSE, pdTRUE, portMAX_DELAY );

#if WIFI_MANAGER_DEBUG
	printf("wifi_mamager: softAP started, starting http_server\n");
#endif
	http_server_set_event_start();


	EventBits_t uxBits;
	for(;;){

		/* actions that can trigger: request a connection, a scan, or a disconnection */
		uxBits = xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT | WIFI_MANAGER_REQUEST_WIFI_SCAN | WIFI_MANAGER_REQUEST_WIFI_DISCONNECT, pdFALSE, pdFALSE, portMAX_DELAY );
		if(uxBits & WIFI_MANAGER_REQUEST_WIFI_DISCONNECT){
			/* user requested a disconnect, this will in effect disconnect the wifi but also erase NVS memory*/

			/*disconnect only if it was connected to begin with! */
			if( uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT ){
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				/* wait until wifi disconnects. From experiments, it seems to take about 150ms to disconnect */
				xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT, pdFALSE, pdTRUE, portMAX_DELAY );
			}
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);

			/* erase configuration */
			if(wifi_manager_config_sta){
				memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
			}

			/* save NVS memory */
			wifi_manager_save_sta_config();

			/* update JSON status */
			if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
				wifi_manager_generate_ip_info_json(UPDATE_USER_DISCONNECT);
				wifi_manager_unlock_json_buffer();
			}
			else{
				/* Even if someone were to furiously refresh a web resource that needs the json mutex,
				 * it seems impossible that this thread cannot obtain the mutex. Abort here is reasonnable.
				 */
				abort();
			}

			/* finally: release the scan request bit */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_DISCONNECT);
		}
		if(uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT) {

			// If the ESP32 is already connected to a access point -> disconnect 
			if( (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) == WIFI_MANAGER_WIFI_CONNECTED_BIT) {

				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				// Wait until wifi disconnects. It take about 150ms to disconnect
				xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
			}
			// Set the new config and connect - reset the disconnect bit first as it is later tested
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
			ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_manager_get_wifi_sta_config()));
			ESP_ERROR_CHECK(esp_wifi_connect());

			/* 2 scenarios here: connection is successful and SYSTEM_EVENT_STA_GOT_IP will be posted
			 * or it's a failure and we get a SYSTEM_EVENT_STA_DISCONNECTED with a reason code.
			 * Note that the reason code is not exploited. For all intent and purposes a failure is a failure.
			 */
			uxBits = xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_STA_DISCONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY );

			if(uxBits & (WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_STA_DISCONNECT_BIT)){

				/* Update the json regardless of connection status.
				 * If connection was succesful an IP will get assigned.
				 * If the connection attempt is failed we mark it as a failed connection attempt
				 * as it is important for the front end app to distinguish failed attempt to
				 * regular disconnects
				 */
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){

					/* only save the config if the connection was successful! */
					if(uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT){

						/* generate the connection info with success */
						wifi_manager_generate_ip_info_json( UPDATE_CONNECTION_OK );

						/* save wifi config in NVS */
						wifi_manager_save_sta_config();
					}
					else{

						/* failed attempt to connect regardles of the reason */
						wifi_manager_generate_ip_info_json( UPDATE_FAILED_ATTEMPT );

						/* otherwise: reset the config */
						memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
					}
					wifi_manager_unlock_json_buffer();
				}
				else{
					/* Even if someone were to furiously refresh a web resource that needs the json mutex,
					 * it seems impossible that this thread cannot obtain the mutex. Abort here is reasonnable.
					 */
					abort();
				}
			}
			else{
				/* hit portMAX_DELAY limit ? */
				abort();
			}

			/* finally: release the connection request bit */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
		}
		else if(uxBits & WIFI_MANAGER_REQUEST_WIFI_SCAN){
                        ap_num = MAX_AP_NUM;

			ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
			ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));

			/* make sure the http server isn't trying to access the list while it gets refreshed */
			if(wifi_manager_lock_json_buffer( ( TickType_t ) 20 )){
				/* Will remove the duplicate SSIDs from the list and update ap_num */
				wifi_manager_filter_unique(accessp_records, &ap_num);
				wifi_manager_generate_acess_points_json();
				wifi_manager_unlock_json_buffer();
			}
			else{
#if WIFI_MANAGER_DEBUG
				printf("wifi_manager: could not get access to json mutex in wifi_scan\n");
#endif
			}

			/* finally: release the scan request bit */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_SCAN);
		}
	} /* for(;;) */
	vTaskDelay( (TickType_t)10);
}


void init_wifi_manager()
{
	// Initialize flash memory
	nvs_flash_init();

	// Start the HTTP server task
	xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);

	// Start the wifi manager task
	xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, 4, NULL);
}