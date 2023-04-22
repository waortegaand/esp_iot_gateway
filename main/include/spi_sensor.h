#ifndef _SPI_SENSOR_H
#define _SPI_SENSOR_H

#define DMA_CHAN    2

#define PIN_NUM_MISO 12//19
#define PIN_NUM_MOSI 13//23
#define PIN_NUM_CLK  14//18
#define PIN_NUM_CS   25

//#define PIN_NUM_DC   4
//#define PIN_NUM_RST  5
//#define PIN_NUM_BCKL 6

spi_device_handle_t spi_init(void);
int16_t read_temp(spi_device_handle_t spiParams);

#endif // _SPI_SENSOR_H
