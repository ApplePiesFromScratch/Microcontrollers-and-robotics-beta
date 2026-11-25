#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t; typedef struct spi_dev *spi_device_handle_t;
typedef struct spi_transaction_t { uint32_t flags; size_t length; void *user; union { const void *tx_buffer; uint8_t tx_data[4]; }; } spi_transaction_t;
#define SPI_TRANS_USE_TXDATA 4
typedef struct { int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num, max_transfer_sz; } spi_bus_config_t;
typedef void (*transaction_cb_t)(spi_transaction_t *);
typedef struct { int clock_speed_hz, mode, spics_io_num, queue_size; transaction_cb_t pre_cb; } spi_device_interface_config_t;
typedef enum { SPI2_HOST = 1 } spi_host_device_t;
#define SPI_DMA_CH_AUTO 3
esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t *, int); esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t *, spi_device_handle_t *);
esp_err_t spi_device_queue_trans(spi_device_handle_t, spi_transaction_t *, uint32_t); esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t **, uint32_t);
