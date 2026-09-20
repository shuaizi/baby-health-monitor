#pragma once

#include <stdint.h>

typedef struct {
    int cores;
    uint32_t features;
    uint16_t revision;
} esp_chip_info_t;

#define CHIP_FEATURE_WIFI_BGN (1U << 0)
#define CHIP_FEATURE_BT (1U << 1)
#define CHIP_FEATURE_BLE (1U << 2)
#define CHIP_FEATURE_IEEE802154 (1U << 3)
#define CHIP_FEATURE_EMB_FLASH (1U << 4)

void esp_chip_info(esp_chip_info_t *info);
