#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize NVS flash and start Wi-Fi connection in STA mode.
 *
 * @param ssid Wi-Fi SSID
 * @param password Wi-Fi Password
 * @return esp_err_t ESP_OK on successful Wi-Fi initialization
 */
esp_err_t wifi_app_init_sta(const char *ssid, const char *password);

/**
 * @brief Check if Wi-Fi is currently connected and assigned an IP address.
 *
 * @return true if connected, false otherwise.
 */
bool wifi_app_is_connected(void);

/**
 * @brief Get assigned IP address as a string.
 *
 * @param buf Buffer to store IP string (at least 16 bytes)
 * @param max_len Size of buffer
 */
void wifi_app_get_ip_string(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif
