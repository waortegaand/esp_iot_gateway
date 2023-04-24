# Embed the server root certificate into the final binary
#
idf_component_register(SRCS "gateway_main.c" "spi_sensor.c" "wifi_connect.c" "mqtt_request.c" "http_request.c"
                    INCLUDE_DIRS "include"
                    EMBED_TXTFILES certs/iotplatform_azurewebsites_net_chain.pem
                    )
