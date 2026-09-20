/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "ssd1306.h"
#include "tvoc301.h"
#include "wifi_app.h"
#include "prometheus_server.h"


#define LED_GPIO GPIO_NUM_48
#define SSD1306_SDA_GPIO GPIO_NUM_8
#define SSD1306_SCL_GPIO GPIO_NUM_9
#define SSD1306_I2C_ADDRESS 0x3c
#define TVOC301_TX_GPIO GPIO_NUM_17
#define TVOC301_RX_GPIO GPIO_NUM_18
#define TVOC301_UART_PORT UART_NUM_1
#define TVOC301_BAUD_RATE 9600

typedef struct {
    ssd1306_device_t *device;
    bool enabled;
} display_state_t;

static void init_led(void)
{
    gpio_config_t led_config = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_config);
}

static void show_display(display_state_t *display)
{
    esp_err_t result = ssd1306_show(display->device);
    if (result == ESP_OK) {
        return;
    }

    display->enabled = false;
    printf("SSD1306 display update failed: %s; display output disabled until restart.\n", esp_err_to_name(result));
    result = ssd1306_deinit(display->device);
    if (result == ESP_OK) {
        display->device = NULL;
    } else {
        printf("SSD1306 resource cleanup failed: %s\n", esp_err_to_name(result));
    }
}

static void draw_reading(display_state_t *display, const tvoc301_reading_t *reading)
{
    if (!display->enabled) {
        return;
    }
    char line[22];

    ssd1306_clear(display->device);
    snprintf(line, sizeof(line), "TVOC:%u.%03u", (unsigned)(reading->tvoc / 1000), (unsigned)(reading->tvoc % 1000));
    ssd1306_draw_text(display->device, 0, 0, line);
    snprintf(line, sizeof(line), "CH2O:%u.%03u", (unsigned)(reading->ch2o / 1000), (unsigned)(reading->ch2o % 1000));
    ssd1306_draw_text(display->device, 0, 12, line);
    snprintf(line, sizeof(line), "CO2 :%u.%03u", (unsigned)(reading->co2 / 1000), (unsigned)(reading->co2 % 1000));
    ssd1306_draw_text(display->device, 0, 24, line);
    show_display(display);
}

static void draw_status(display_state_t *display, const char *line1, const char *line2, const char *line3)
{
    if (!display->enabled) {
        return;
    }
    ssd1306_clear(display->device);
    ssd1306_draw_text(display->device, 0, 0, line1);
    ssd1306_draw_text(display->device, 0, 12, line2);
    ssd1306_draw_text(display->device, 0, 24, line3);
    show_display(display);
}

static display_state_t init_display(void)
{
    display_state_t display = {0};
#if CONFIG_BABY_HEALTH_DISPLAY_ENABLED
    const ssd1306_config_t display_config = {
        .sda_pin = SSD1306_SDA_GPIO,
        .scl_pin = SSD1306_SCL_GPIO,
        .i2c_address = SSD1306_I2C_ADDRESS,
        .i2c_clock_hz = 400000,
    };
    esp_err_t result = ssd1306_init(&display.device, &display_config);

    if (result != ESP_OK) {
        if (result == ESP_ERR_NOT_FOUND) {
            printf("No display detected at I2C address 0x%02x; display output disabled until restart.\n", SSD1306_I2C_ADDRESS);
        } else {
            printf("SSD1306 initialization failed: %s; display output disabled until restart.\n", esp_err_to_name(result));
        }
        return display;
    }

    display.enabled = true;
    draw_status(&display, "BABY HEALTH", "TVOC READY", "WAIT DATA");
#else
    printf("SSD1306 display disabled by configuration; skipping I2C initialization.\n");
#endif
    return display;
}

static esp_err_t init_tvoc_sensor(void)
{
    const tvoc301_config_t tvoc_config = {
        .uart_port = TVOC301_UART_PORT,
        .rx_pin = TVOC301_RX_GPIO,
        .tx_pin = TVOC301_TX_GPIO,
        .baud_rate = TVOC301_BAUD_RATE,
    };

    return tvoc301_init(&tvoc_config);
}

static void init_network(void)
{
    if (CONFIG_BABY_HEALTH_WIFI_SSID[0] == '\0') {
        printf("Wi-Fi is not configured; skipping Wi-Fi and HTTP metrics server. Configure Baby Health Monitor in menuconfig.\n");
        return;
    }

    printf("Starting Wi-Fi station...\n");
    esp_err_t result = wifi_app_init_sta(CONFIG_BABY_HEALTH_WIFI_SSID, CONFIG_BABY_HEALTH_WIFI_PASSWORD);
    if (result != ESP_OK) {
        printf("Wi-Fi initialization failed: %s; HTTP metrics server not started.\n", esp_err_to_name(result));
        return;
    }

    result = prometheus_server_start();
    if (result != ESP_OK) {
        printf("Prometheus HTTP server initialization failed: %s\n", esp_err_to_name(result));
    }
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

    init_led();
    display_state_t display = init_display();
    esp_err_t result = init_tvoc_sensor();
    if (result != ESP_OK) {
        printf("TVOC301 initialization failed: %s\n", esp_err_to_name(result));
        draw_status(&display, "TVOC INIT", "FAILED", esp_err_to_name(result));
        return;
    }

    init_network();

    while (true) {
        tvoc301_reading_t reading;

        result = tvoc301_read(&reading, 3000);
        if (result == ESP_OK) {
            printf("TVOC=%u, CH2O=%u, CO2=%u\n", reading.tvoc, reading.ch2o, reading.co2);
            gpio_set_level(LED_GPIO, 1);
            prometheus_server_update_reading(&reading, true);
            draw_reading(&display, &reading);
        } else {
            printf("TVOC301 read failed: %s\n", esp_err_to_name(result));
            gpio_set_level(LED_GPIO, 0);
            prometheus_server_update_reading(NULL, false);
            draw_status(&display, "TVOC TIMEOUT", "CHECK UART", "RX GPIO18");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
