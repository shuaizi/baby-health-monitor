/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "ssd1306.h"

#define LED_GPIO GPIO_NUM_48
#define LED_BLINK_COUNT 10
#define LED_INTERVAL_MS 1000
#define SSD1306_SDA_GPIO GPIO_NUM_8
#define SSD1306_SCL_GPIO GPIO_NUM_9
#define SSD1306_I2C_ADDRESS 0x3c

static void blink_led(void)
{
    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_config);

    for (int i = 0; i < LED_BLINK_COUNT; i++) {
        gpio_set_level(LED_GPIO, 1);
        printf("LED on (%d/%d)\n", i + 1, LED_BLINK_COUNT);
        vTaskDelay(pdMS_TO_TICKS(LED_INTERVAL_MS));

        gpio_set_level(LED_GPIO, 0);
        printf("LED off (%d/%d)\n", i + 1, LED_BLINK_COUNT);
        vTaskDelay(pdMS_TO_TICKS(LED_INTERVAL_MS));
    }
}

static void show_display_demo(void)
{
    const ssd1306_config_t display_config = {
        .sda_pin = SSD1306_SDA_GPIO,
        .scl_pin = SSD1306_SCL_GPIO,
        .i2c_address = SSD1306_I2C_ADDRESS,
        .i2c_clock_hz = 400000,
    };
    ssd1306_device_t *display;
    esp_err_t result = ssd1306_init(&display, &display_config);

    if (result != ESP_OK) {
        printf("SSD1306 initialization failed: %s\n", esp_err_to_name(result));
        return;
    }

    ssd1306_clear(display);
    ssd1306_draw_text(display, 0, 0, "BABY HEALTH");
    ssd1306_draw_text(display, 0, 12, "SSD1306 READY");
    ssd1306_draw_text(display, 0, 24, "I2C: 0X3C");
    result = ssd1306_show(display);
    printf("SSD1306 display update: %s\n", esp_err_to_name(result));
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    show_display_demo();
    blink_led();

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
