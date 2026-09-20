#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

typedef struct {
    uart_port_t uart_port;
    gpio_num_t rx_pin;
    gpio_num_t tx_pin;
    int baud_rate;
} tvoc301_config_t;

typedef struct {
    uint16_t tvoc;
    uint16_t ch2o;
    uint16_t co2;
} tvoc301_reading_t;

esp_err_t tvoc301_init(const tvoc301_config_t *config);
esp_err_t tvoc301_read(tvoc301_reading_t *reading, uint32_t timeout_ms);
