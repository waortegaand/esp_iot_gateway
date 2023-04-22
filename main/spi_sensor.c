#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "spi_sensor.h"

#define TAG "MAX6675 Temperature Sensor"

/* SPI Config */
spi_device_handle_t spi_init(void) {
  spi_device_handle_t spi;
  esp_err_t ret;
  spi_bus_config_t buscfg = {
    .miso_io_num = PIN_NUM_MISO,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = (4 * 8)
  };


  //Initialize the SPI bus
  ret = spi_bus_initialize(VSPI_HOST, &buscfg, DMA_CHAN);
  ESP_ERROR_CHECK(ret);

  spi_device_interface_config_t devCfg={
    .mode = 0,
    .clock_speed_hz = 2*1000*1000,
    .spics_io_num=25,
    .queue_size=3
  };

  ret = spi_bus_add_device(VSPI_HOST, &devCfg, &spi);
  ESP_ERROR_CHECK(ret);

  return spi;
}

int16_t read_temp(spi_device_handle_t spiParams){
  spi_device_handle_t spi = spiParams;

  uint16_t data;
  spi_transaction_t tM = {
    .tx_buffer = NULL,
    .rx_buffer = &data,
    .length = 16,
    .rxlength = 16,
  };

  spi_device_acquire_bus(spi, portMAX_DELAY);
  spi_device_transmit(spi, &tM);
  spi_device_release_bus(spi);

  int16_t res = (int16_t) SPI_SWAP_DATA_RX(data, 16);

  if (res & (1 << 2))
    ESP_LOGE(TAG, "Sensor is not connected\n");
  else {
    res >>= 3;
    //printf("SPI res = %d temp=%f\n", res, res * 0.25);
  }
  //vTaskDelay(pdMS_TO_TICKS(1000));

  return res;
}