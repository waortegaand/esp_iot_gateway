#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

// Other 'include'
#include "wifi_connect.h"
#include "spi_sensor.h"
// 'include' hidden
#include "iot_config.h"

static const char *TAG_WIFI = "___wifi_station___";

/* Sensor variables task */
static int16_t temperature = 0;
static uint16_t timePeriodo = 1000;

void temp_task(void * pvParameters) {
  spi_device_handle_t spi = (spi_device_handle_t) pvParameters;
  while(true)
  {
    temperature = read_temp(spi);
    printf("SPI Temp=%f\n", temperature * 0.25);
    vTaskDelay(timePeriodo / portTICK_PERIOD_MS);
    //counter_t++;
  }
  vTaskDelete(NULL);
}


void app_main(void)
{
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
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	
	xTaskCreate( &temp_task, "temperature_task", 4096, spi, 5, NULL );
}
