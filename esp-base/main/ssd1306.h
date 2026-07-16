#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint8_t i2c_address;
    uint32_t i2c_clock_hz;
} ssd1306_config_t;

typedef struct ssd1306_device ssd1306_device_t;

esp_err_t ssd1306_init(ssd1306_device_t **device, const ssd1306_config_t *config);
void ssd1306_clear(ssd1306_device_t *device);
void ssd1306_draw_text(ssd1306_device_t *device, uint8_t x, uint8_t y, const char *text);
esp_err_t ssd1306_show(ssd1306_device_t *device);
