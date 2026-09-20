#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "tvoc301.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the HTTP server serving Prometheus metrics on /metrics.
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t prometheus_server_start(void);

/**
 * @brief Update the internal sensor reading state (thread-safe).
 *
 * @param reading Pointer to latest tvoc301_reading_t
 * @param is_online True if sensor read was successful, false if read error/timeout
 */
void prometheus_server_update_reading(const tvoc301_reading_t *reading, bool is_online);

#ifdef __cplusplus
}
#endif
