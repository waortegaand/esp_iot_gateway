/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "esp_http_client.h"

#include "http_request.h"
#include "mqtt_request.h"
#include "iot_config.h"

/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/

static int status_code = 0;

/* Certificate is used in this project for https web page */
extern const char azurewebsites_net_chain_pem_start[] asm("_binary_iotplatform_azurewebsites_net_chain_pem_start");
extern const char azurewebsites_net_chain_pem_end[]   asm("_binary_iotplatform_azurewebsites_net_chain_pem_end");


static const char *TAG = "___HTTP_REQUEST_C___ :  ";
//static const char *WIFI_TASK_NAME = "HTTP_CLIENT_EXAMPLE: ";

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                // printf("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

//void https_rest_with_url_post(void);
esp_http_client_config_t config = {
    .host = HOST_IOT_SERVER,
    .path = "/",
    .event_handler = _http_event_handler,
    //.cert_pem = azurewebsites_net_chain_pem_start,
};

void http_get(void){
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_http_client_set_url(client, PATH_GET_LAST_TEMP);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        printf("\n--------------- GET Message Obtained -------------\n");
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void http_post(const char *send_data){
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // POST
    const char *post_data = send_data;
    esp_http_client_set_url(client, PATH_POST_TEMP);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    printf("\n--------------- POST Message: Send (%s)-----------\n", post_data);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                status_code,
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

int64_t get_time_http_msn(int16_t temp){
	static char msnSend[40];
	sprintf(msnSend,"{\"tempTk\":\"%f\"}",(temp*0.25));
	ESP_LOGI(TAG, "Peticion #: %f ... mensaje: %s", (temp*0.25), msnSend);
	//vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);	

	http_post(msnSend);
	//http_get();
	int64_t time_tr = esp_timer_get_time();
	//status_code = 0;
	return time_tr;
}

/*
 *  http_native_request() demonstrates use of low level APIs to connect to a server,
 *  make a http request and read response. Event handler is not used in this case.
 *  Note: This approach should only be used in case use of low level APIs is required.
 *  The easiest way is to use esp_http_perform()
 */
