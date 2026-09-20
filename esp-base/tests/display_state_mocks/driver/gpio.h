#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef int gpio_num_t;
typedef struct {
    uint64_t pin_bit_mask;
    int mode;
} gpio_config_t;

#define GPIO_NUM_8 8
#define GPIO_NUM_9 9
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_48 48
#define GPIO_MODE_OUTPUT 1

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
