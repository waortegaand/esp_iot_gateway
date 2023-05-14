/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

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
#include "esp_timer.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "iot_config.h"
#include "mqtt_request.h"

/* Definitions located in "esp_config.h"
  #define URI_MQTT 	""
  #define TOPIC_PUB_TEMP	""		// Temperature Data
  #define TOPIC_SUB_TIME	""		// Sampling time
  #define TOPIC_SUB_NREQ	""
  #define TOPIC_SUB_CONTROL	""
*/

static const char *TAG = "__MQTT_REQUEST_C__ : ";
static esp_mqtt_client_handle_t client = NULL;
//static char count[20];
static int32_t timesend = 5000;
static int32_t numreq = 100;
static int32_t numcontrol = 1;
static bool reqmsn = false;

static int32_t str_to_int(const char *str, int32_t len)
{
    int32_t num = 0;
    int32_t i = 0;
    while (str[i] && (str[i] >= '0' && str[i] <= '9') && (i < len))
    {
        num = num * 10 + (str[i] - '0');
        i++;
    }
    return num;
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, TOPIC_PUB_TEMP, 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, TOPIC_SUB_TIME, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, TOPIC_SUB_NREQ, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, TOPIC_SUB_CONTROL, 0); /* Control enable or disable request */
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //msg_id = esp_mqtt_client_publish(client, "/esp32/test/temp", "data", 0, 0, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        if (strncmp(event->topic, TOPIC_PUB_TEMP, event->topic_len) == 0){
            reqmsn = true;
        }/**/
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        ESP_LOGI(TAG, "Mensajes - ALL EVENT: msg_id=%d", event->msg_id);
        if (strncmp(event->topic, TOPIC_SUB_TIME, event->topic_len) == 0)
        {
            timesend = str_to_int(event->data, event->data_len);
            ESP_LOGI(TAG, "Nuevo valor de tiempo %d", timesend);
        }
        else if (strncmp(event->topic, TOPIC_SUB_NREQ, event->topic_len) == 0)
        {
            numreq = str_to_int(event->data, event->data_len);
            ESP_LOGI(TAG, "Nuevo valor de peticiones %d.", numreq);
        }
        else if (strncmp(event->topic, TOPIC_SUB_CONTROL, event->topic_len) == 0)
        {
            numcontrol = str_to_int(event->data, event->data_len);
            ESP_LOGI(TAG, "Enable or Disable Control %d.", numcontrol);
        }else if (strncmp(event->topic, TOPIC_PUB_TEMP, event->topic_len) == 0)
        {
            ESP_LOGI(TAG, "Mensaje id: msg_id=%d", event->msg_id);
            //reqmsn = true;
        }/**/
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

int32_t get_num_control(void)
{
    return numcontrol;
}

void set_num_control(void)
{
    numcontrol = 100;
}

int32_t get_time_ms()
{
    return timesend;
}

int32_t get_req_num()
{
    return numreq;
}

int64_t get_time_mqtt_msn(int16_t temp)
{
    //static bool flag = false;
    static char sendTemp[40];
    sprintf(sendTemp, "{\"tempTk\":\"%f\"}", (temp * 0.25));
    int msg_id = esp_mqtt_client_publish(client, TOPIC_PUB_TEMP, sendTemp, 0, 1, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d, temp=%s", msg_id, sendTemp);
    //ESP_LOGI(TAG, "Numero de publicacion: %d PUB", counter);
    while(true){
        if(reqmsn==true){
            int64_t time_tr = esp_timer_get_time();
            reqmsn = false;
            return time_tr;
        }
    }
}

esp_mqtt_client_handle_t mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = URI_MQTT,
        .port = 1883,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    return client;
}
