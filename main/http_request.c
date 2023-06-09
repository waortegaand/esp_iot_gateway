/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
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

static int32_t timesend = 5000;
static int status_code = 0;
static int32_t numTemp = 0;
static int16_t temperature = 0;

/* Certificate is used in this project for https web page */
extern const char azurewebsites_net_chain_pem_start[] asm("_binary_iotplatform_azurewebsites_net_chain_pem_start");
extern const char azurewebsites_net_chain_pem_end[] asm("_binary_iotplatform_azurewebsites_net_chain_pem_end");

static const char *TAG = "___HTTP_REQUEST_C___ :  ";
static const char *TAG_GET = "___REQUEST_C_SERIAL_GET___ :  ";
static const char *TAG_POST = "___REQUEST_C_SERIAL_POST___ :  ";
static const char *TAG_EVENT = "___EVENT_ON_DATA_REQUEST_C___ :  ";
//static const char *WIFI_TASK_NAME = "HTTP_CLIENT_EXAMPLE: ";

static int32_t str_to_int(const char *str, int32_t len)
{
    int32_t num = 0;
    int32_t i = 0;
    for(i=0;i<len;i++)
    {
        if(str[i] >= 48 && str[i] <= 57){
            num = num*10 + (str[i] - '0');
        }
    }
    return num;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    //static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
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
        ESP_LOGD(TAG_EVENT, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            if (evt->user_data) {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            }
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
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
    .cert_pem = azurewebsites_net_chain_pem_start,
};

void http_get(void)
{
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_http_client_set_url(client, PATH_GET_LAST_TEMP);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        printf("\n--------------- GET Message Obtained -------------\n");
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void http_post(const char *send_data)
{
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // POST
    const char *post_data = send_data;
    esp_http_client_set_url(client, PATH_POST_TEMP);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    printf("\n--------------- POST Message: Send (%s)-----------\n", post_data);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 status_code,
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
/*
void http_post_series(void){
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    static char msnSend[120];
    int32_t newNum = numTemp;
    int32_t count = 0;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    while(count<10){
        int64_t time_t0, time_t1, time_td;
        if(newNum == numTemp){
            time_t0 = esp_timer_get_time();
            sprintf(msnSend,"{\"temp\":%f,\"pos\":%d}",(temperature*0.25),numTemp);
            //
            // POST
            const char *post_data = msnSend;
            esp_http_client_set_url(client, PATH_POST_SERIAL);
            esp_http_client_set_method(client, HTTP_METHOD_POST);
            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, post_data, strlen(post_data));
            printf("\n--------------- SERIAL POST Message: Send (%s)-----------\n", post_data);
            esp_err_t err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                status_code = esp_http_client_get_status_code(client);
                ESP_LOGI(TAG_POST, "SERIAL POST Status = %d, content_length = %d",
                    status_code,
                    esp_http_client_get_content_length(client));
            } else {
                ESP_LOGE(TAG_POST, "SERIAL POST request failed: %s", esp_err_to_name(err));
            }
            time_t1 = esp_timer_get_time();
            time_td = time_t1 - time_t0;
            ESP_LOGI(TAG_POST, "SERIAL POST Time Response = %lld", time_td);
            newNum++;
            count++;
        }
    }
    esp_http_client_cleanup(client);
    ESP_LOGE(TAG_POST, "SERIAL POST End Conection ");
}
*/

void http_post_series(void){
    int64_t time_t0, time_t1, time_tr;
    time_t0 = esp_timer_get_time();
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    static char msnSend[120];
    //int32_t newNum = numTemp;
    //int32_t count = 0;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    sprintf(msnSend, "{\"temp\":%f,\"pos\":%d}", (temperature * 0.25), numTemp);
    //
    // POST
    const char *post_data = msnSend;
    esp_http_client_set_url(client, PATH_POST_SERIAL);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    printf("\n--------------- SERIAL POST Message: Send (%s)-----------\n", post_data);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_POST, "SERIAL POST Status = %d, content_length = %d",
                 status_code,
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG_POST, "SERIAL POST request failed: %s", esp_err_to_name(err));
    }
    //time_t1 = esp_timer_get_time();
    //time_td = time_t1 - time_t0;
    //ESP_LOGI(TAG_POST, "SERIAL POST Time Response = %lld", time_td);
    esp_http_client_cleanup(client);
    ESP_LOGE(TAG_POST, "SERIAL POST End Conection ");
    time_t1 = esp_timer_get_time();
    time_tr = time_t1 - time_t0;
    ESP_LOGE(TAG_POST, "______Time : %lld", time_tr);
}

int32_t http_get_time(void)
{
    int64_t time_t0, time_t1, time_tr;
    time_t0 = esp_timer_get_time();
    char local_response_buffer[32] = {0};
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
        .user_data = local_response_buffer, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_url(client, PATH_GET_SERIAL);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        //ESP_LOGI(TAG_GET, "SERIAL GET TIME Status = %d, content_length = %d",
        //         esp_http_client_get_status_code(client),
        //         esp_http_client_get_content_length(client));
        //printf("%s\n",local_response_buffer);
        timesend = str_to_int(local_response_buffer,strlen(local_response_buffer));
        printf("TIMESEND %d \n", timesend);
    }
    else
    {
        ESP_LOGE(TAG_GET, "SERIAL GET TIME request failed: %s", esp_err_to_name(err));
    }
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_http_client_cleanup(client);
    time_t1 = esp_timer_get_time();
    time_tr = time_t1 - time_t0;
    ESP_LOGE(TAG_GET, "______Time : %lld", time_tr);
    return timesend;
}

void http_set_time(const char *send_data){
    int64_t time_t0, time_t1, time_tr;
    time_t0 = esp_timer_get_time();
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        .path = "/",
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    //
    // POST
    const char *post_data = send_data;
    esp_http_client_set_url(client, PATH_SET_SERIAL);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    printf("\n--------------- NEW TIME SEND POST Message: Send (%s)-----------\n", post_data);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_POST, "NEW SERIAL POST Status = %d, content_length = %d",
                 status_code,
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG_POST, "SERIAL POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    ESP_LOGE(TAG_POST, "SERIAL POST End Conection ");
    time_t1 = esp_timer_get_time();
    time_tr = time_t1 - time_t0;
    ESP_LOGE(TAG_POST, "______Time : %lld", time_tr);
}
/**/

/*
void http_get_time(void){
    char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc http receive buffer");
        return;
    }
    esp_http_client_config_t config = {
        .host = HOST_IOT_SERVER,
        //.url = "https://"HOST_IOT_SERVER,
        .path = PATH_GET_SERIAL,
        .event_handler = _http_event_handler,
        .cert_pem = azurewebsites_net_chain_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    // GET
    //esp_http_client_set_method(client, HTTP_METHOD_GET);
    //esp_http_client_set_url(client, PATH_GET_SERIAL);
    //
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_GET, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(buffer);
        return;
    }
    int content_length =  esp_http_client_fetch_headers(client);
    int total_read_len = 0, read_len;
    if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER) {
        read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len <= 0) {
            ESP_LOGE(TAG_GET, "Error read data");
        }
        buffer[read_len] = 0;
        ESP_LOGD(TAG_GET, "read_len = %d", read_len);
    }
    ESP_LOGI(TAG_GET, "HTTP Stream reader Status = %d, content_length = %d",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}
*/
int32_t get_time_period(void)
{
    if (timesend < 100)
    {
        return 5000;
    }
    return timesend;
}

int64_t get_time_http_msn(int16_t temp)
{
    static char msnSend[40];
    sprintf(msnSend, "{\"tempTk\":\"%f\"}", (temp * 0.25));
    ESP_LOGI(TAG, "Peticion #: %f ... mensaje: %s", (temp * 0.25), msnSend);
    //vTaskDelay(get_time_ms() / portTICK_PERIOD_MS);
    http_post(msnSend);
    //http_get();
    int64_t time_tr = esp_timer_get_time();
    //status_code = 0;
    return time_tr;
}

void set_http_data(int32_t counter, int16_t temp)
{
    numTemp = counter;
    temperature = temp;
}
/*
 *  http_native_request() demonstrates use of low level APIs to connect to a server,
 *  make a http request and read response. Event handler is not used in this case.
 *  Note: This approach should only be used in case use of low level APIs is required.
 *  The easiest way is to use esp_http_perform()
 */
