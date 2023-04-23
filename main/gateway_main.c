#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
//#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/spi_master.h"

// Other 'include'
#include "wifi_connect.h"
#include "spi_sensor.h"
#include "mqtt_request.h"
// 'include' hidden
#include "iot_config.h"

static const char *TAG_WIFI = "___wifi_station___";
static const char *TAG_CONTROL = "___CONTROL:___";
static const char *TAG_IOT = "___Main_IoT___";

/* "TaskHandle_t xHandle;" for each task definition  */
TaskHandle_t xHandle_control = NULL;
TaskHandle_t xHandle_sensor = NULL;
TaskHandle_t xHandle_mqtt = NULL;

/* Sensor variables task */
static int16_t temperature = 0;
static int32_t timePeriodo = 1000;
static int32_t  msncontrol = 0;

/* MQTT variables */
static const char *TAG_MQTT = "MQTT_MAIN";
static char sendTemp[20];
static esp_mqtt_client_handle_t client = NULL;

/* Task: Selection Control - Enable-Disable mqtt and http request*/

void control_task(void * pvParameters){
	int msg_id = esp_mqtt_client_subscribe(client, TOPIC_SUB_CONTROL, 0);	/* Control enable or disable request */
	ESP_LOGI(TAG_CONTROL, "Suscrito Topic CONTROL, msg_id=%d", msg_id);
	while(true){
		msncontrol = get_num_control();
		if(msncontrol == 0){
			vTaskSuspend(xHandle_mqtt);
			set_num_control();
		}else if(msncontrol == 5){
			vTaskResume(xHandle_mqtt);
			set_num_control();
		}		
		vTaskDelay(100 / portTICK_PERIOD_MS); // 10 ms
	}
}


/* Task: SPI MAX6675 - Temperature Sensor Thermocouple K  */
void temp_task(void * pvParameters) {
	spi_device_handle_t spi = (spi_device_handle_t) pvParameters;
	while(true)
	{
		temperature = read_temp(spi);
		printf("SPI Temp=%f\n", temperature * 0.25);
		//vTaskDelay(timePeriodo / portTICK_PERIOD_MS);
		vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);
	}
}

/* Task: MQTT Publish - Value Temperature Sensor */
void pub_mqtt_task(void *pvParameter){
	int counter = 0;
	vTaskDelay(150 / portTICK_PERIOD_MS); // 10 ms
	while(counter<10){
		counter++;
		sprintf(sendTemp,"%f",(temperature*0.25));
		ESP_LOGI(TAG_MQTT, "Numero de publicacion: %d PUB", counter);
		
        int msg_id = esp_mqtt_client_publish(client, TOPIC_PUB_TEMP, sendTemp, 0, 1, 0);
        ESP_LOGI(TAG_MQTT, "sent publish successful, msg_id=%d, temp=%s", msg_id, sendTemp);
        vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);	
        
        if(counter >= 10){
			vTaskSuspend(NULL);
			counter = 0;
        }
	}
}

void app_main(void)
{

    ESP_LOGI(TAG_IOT, "[APP] Startup..");
    ESP_LOGI(TAG_IOT, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG_IOT, "[APP] IDF version: %s", esp_get_idf_version());
    
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
    
	ESP_ERROR_CHECK(ret);
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
	ESP_ERROR_CHECK(example_connect());
	ESP_LOGI(TAG_WIFI, "Connected to AP, begin http example");
	
	spi_device_handle_t spi;
	spi = spi_init();
	
	client = mqtt_app_start();
	
	//TaskHandle_t xHandle_control;
	//TaskHandle_t xHandle_sensor;
	//TaskHandle_t xHandle_mqtt;
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	xTaskCreate( &control_task, "control_task", 1024*4, NULL, 6, &xHandle_control );
	xTaskCreate( &temp_task, "temperature_task", 4096, spi, 5, &xHandle_sensor );
	xTaskCreate( &pub_mqtt_task, "counter_task", 2048, client, 5, &xHandle_mqtt );
}
