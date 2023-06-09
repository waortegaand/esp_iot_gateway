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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_timer.h"

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
#include "http_request.h"
// 'include' hidden
#include "iot_config.h"
// TAGs
static const char *TAG_HTTP = "___HTTP_MAIN___";
static const char *TAG_MQTT = "___MQTT_MAIN___";
static const char *TAG_WIFI = "___WiFi_station___";
static const char *TAG_CONTROL = "___CONTROL:___";
static const char *TAG_IOT = "___IoT_MAIN___";

/* "TaskHandle_t xHandle;" for each task definition  */
TaskHandle_t xHandle_control = NULL;
TaskHandle_t xHandle_sensor = NULL;
TaskHandle_t xHandle_mqtt = NULL;
TaskHandle_t xHandle_http = NULL;

/* Sensor variables task */
static int16_t temperature = 0;
//static int32_t timePeriodo = 1000;

/* MQTT variables */
//static char sendTemp[20];
static esp_mqtt_client_handle_t client = NULL;
/* HTTP variables */
//static char msnSend[40];

/* Task: Selection Control - Enable-Disable mqtt and http request*/

	static int32_t  msncontrol = 0;
void control_task(void * pvParameters){
	ESP_LOGI(TAG_CONTROL, "Task CONTROL . msncontrol: %d", msncontrol);
	while(true){
		msncontrol = get_num_control();
		if(msncontrol == 0){
			vTaskSuspend(xHandle_mqtt);
			vTaskSuspend(xHandle_http);
			set_num_control();
			ESP_LOGI(TAG_CONTROL, "\n_____CONTROL: vTaskSuspend(xHandle_.....) ALL \n");
		}else if(msncontrol == 5){
			vTaskResume(xHandle_mqtt);
			set_num_control();
			ESP_LOGI(TAG_CONTROL, "\n_____CONTROL: MQTT - vTaskResume(xHandle_mqtt)\n");
		}else if(msncontrol == 10){
			vTaskResume(xHandle_http);
			set_num_control();
			ESP_LOGI(TAG_CONTROL, "\n_____CONTROL: HTTP - vTaskResume(xHandle_http)\n");
		}else if(msncontrol == 200){
			vTaskDelete(xHandle_mqtt);
			vTaskDelete(xHandle_http);
			set_num_control();
			ESP_LOGI(TAG_CONTROL, "\n_____TASK DELETE: vTaskDelete(NULL)\n");
		}
		vTaskDelay(10 / portTICK_PERIOD_MS); // 10 ms
	}
	vTaskDelete(NULL);
}


/* Task: SPI MAX6675 - Temperature Sensor Thermocouple K  */
void sensor_task(void * pvParameters) {
	//vTaskDelay(2000 / portTICK_PERIOD_MS); // 2 s
	spi_device_handle_t spi = (spi_device_handle_t) pvParameters;
	while(true)
	{
		temperature = read_temp(spi);
		printf("SPI Temp=%f\n", temperature * 0.25);
		//vTaskDelay(timePeriodo / portTICK_PERIOD_MS);
		vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

/* Task: MQTT Publish - Value Temperature Sensor */
void pub_mqtt_task(void *pvParameter){
	//vTaskDelay(2000 / portTICK_PERIOD_MS); // 2 s
	int counter = 0;
	while(counter<100){
		counter++;
		int64_t time_t0 = esp_timer_get_time();
		int64_t time_t1 = get_time_mqtt_msn(temperature);
		ESP_LOGI(TAG_MQTT, "Time = %lld", (time_t1-time_t0));
		vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);	
        
		if(counter >= 100){
			ESP_LOGI(TAG_MQTT, "_____Suspend Task MQTT");
			vTaskSuspend(NULL);
			counter = 0;
		}
	}
	ESP_LOGI(TAG_MQTT, "Finish MQTT example");
	vTaskDelete(NULL);
}


/* Task: HTTP Request - Value Temperature Sensor */
static void req_http_task(void *pvParameters)
{
	int counter = 0;
	while(counter<100){
		counter++;
		int64_t time_t0 = esp_timer_get_time();
		int64_t time_t1 = get_time_http_msn(temperature);
		ESP_LOGI(TAG_HTTP, "Time = %lld", (time_t1-time_t0));
		vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);	
        
		if(counter >= 100){
			ESP_LOGI(TAG_HTTP, "_____Suspend Task HTTP");
			vTaskSuspend(NULL);
			counter = 0;
		}
	}
	ESP_LOGI(TAG_HTTP, "Finish HTTP example");
	vTaskDelete(NULL);
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
	
	xTaskCreate( &control_task, 	"control_task",		1024*4, NULL, 	6, &xHandle_control );
	vTaskDelay(5000 / portTICK_PERIOD_MS);
	xTaskCreate( &sensor_task, 	"sensor_task",		1024*4, spi, 	5, &xHandle_sensor );
	xTaskCreate( &pub_mqtt_task, 	"mqtt_task", 		1024*4, client, 5, &xHandle_mqtt );
	xTaskCreate( &req_http_task, 	"http_task", 		1024*8, NULL, 	5, &xHandle_http );
}
