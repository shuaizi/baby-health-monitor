#include "tvoc301.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TVOC301_FRAME_LENGTH 9
#define TVOC301_FRAME_HEADER_1 0x2c
#define TVOC301_FRAME_HEADER_2 0xe4
#define TVOC301_UART_RX_BUFFER_SIZE 1024

static uart_port_t sensor_uart_port = UART_NUM_1;

static uint8_t checksum(const uint8_t *frame)
{
    uint16_t sum = 0;

    for (int i = 0; i < TVOC301_FRAME_LENGTH - 1; i++) {
        sum += frame[i];
    }
    return (uint8_t)sum;
}

static bool parse_frame(const uint8_t *frame, tvoc301_reading_t *reading)
{
    if (frame[0] != TVOC301_FRAME_HEADER_1 || frame[1] != TVOC301_FRAME_HEADER_2) {
        return false;
    }
    if (checksum(frame) != frame[8]) {
        return false;
    }

    reading->tvoc = ((uint16_t)frame[2] << 8) | frame[3];
    reading->ch2o = ((uint16_t)frame[4] << 8) | frame[5];
    reading->co2 = ((uint16_t)frame[6] << 8) | frame[7];
    return true;
}

esp_err_t tvoc301_init(const tvoc301_config_t *config)
{
    const uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t result;

    sensor_uart_port = config->uart_port;
    result = uart_driver_install(sensor_uart_port, TVOC301_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (result != ESP_OK) {
        return result;
    }
    result = uart_param_config(sensor_uart_port, &uart_config);
    if (result != ESP_OK) {
        return result;
    }

    result = uart_set_pin(sensor_uart_port, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (result != ESP_OK) {
        return result;
    }

    if (config->tx_pin != UART_PIN_NO_CHANGE && (int)config->tx_pin >= 0) {
        // Send command to switch sensor to Active Upload Mode: FF 00 78 40 00 00 00 00 48
        const uint8_t active_mode_cmd[] = {0xFF, 0x00, 0x78, 0x40, 0x00, 0x00, 0x00, 0x00, 0x48};
        printf("TVOC301: Sending active upload mode command to sensor...\n");
        uart_write_bytes(sensor_uart_port, (const char *)active_mode_cmd, sizeof(active_mode_cmd));
        vTaskDelay(pdMS_TO_TICKS(100)); // Allow sensor to process the command
    }

    return ESP_OK;
}

esp_err_t tvoc301_read(tvoc301_reading_t *reading, uint32_t timeout_ms)
{
    // Flush input buffer to ensure we align with the newest data frame
    uart_flush_input(sensor_uart_port);

    uint8_t frame[TVOC301_FRAME_LENGTH] = {0};
    int frame_index = 0;
    int total_bytes_received = 0;
    int checksum_failures = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        uint8_t byte;
        int length = uart_read_bytes(sensor_uart_port, &byte, 1, pdMS_TO_TICKS(50));

        if (length <= 0) {
            continue;
        }

        total_bytes_received++;

        if (frame_index == 0 && byte != TVOC301_FRAME_HEADER_1) {
            continue;
        }
        if (frame_index == 1 && byte != TVOC301_FRAME_HEADER_2) {
            frame_index = byte == TVOC301_FRAME_HEADER_1 ? 1 : 0;
            frame[0] = TVOC301_FRAME_HEADER_1;
            continue;
        }

        frame[frame_index++] = byte;
        if (frame_index == TVOC301_FRAME_LENGTH) {
            if (parse_frame(frame, reading)) {
                return ESP_OK;
            }
            checksum_failures++;
            frame_index = 0;
        }
    }

    // Print helpful troubleshooting diagnostics
    printf("TVOC301 Read Timeout: received %d bytes, checksum failures: %d\n",
           total_bytes_received, checksum_failures);
    if (total_bytes_received == 0) {
        printf("  [Diagnostics] No data received. Check wiring (ESP32 RX pin -> Sensor TX pin) and check that the sensor has a stable 5V VCC and Ground connected.\n");
    } else if (checksum_failures > 0) {
        printf("  [Diagnostics] Received data but checksums failed. Check serial line noise, logic levels, or sensor compatibility.\n");
    } else {
        printf("  [Diagnostics] Received %d bytes, but failed to find a valid frame header (expected 0x2C 0xE4). Check baud rate or sensor protocol/model.\n", total_bytes_received);
    }

    return ESP_ERR_TIMEOUT;
}
