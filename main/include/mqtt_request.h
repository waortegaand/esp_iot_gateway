#include <stdio.h>
#include "mqtt_client.h"

#ifndef __MQTT_REQUEST__
#define __MQTT_REQUEST__

esp_mqtt_client_handle_t mqtt_app_start(void);
int32_t get_time_ms();
int32_t get_req_num();
int32_t mqtt_get_time();
int32_t get_num_control(void);
int64_t get_time_mqtt_msn(int16_t temp);
int32_t get_time_control(void);
void set_num_control(void);
void mqtt_pub_series(void);
void set_mqtt_data(int32_t counter, int16_t temp);
void mqtt_set_time(const char *send_data);

#endif /* __MQTT_REQUEST__ */
