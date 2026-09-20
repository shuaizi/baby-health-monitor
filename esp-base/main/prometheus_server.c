#include "prometheus_server.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "PROM_SERVER";

static httpd_handle_t s_server = NULL;
static SemaphoreHandle_t s_lock = NULL;

static tvoc301_reading_t s_latest_reading = {0};
static bool s_sensor_online = false;

void prometheus_server_update_reading(const tvoc301_reading_t *reading, bool is_online)
{
    if (s_lock == NULL) return;

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (reading != NULL) {
            s_latest_reading = *reading;
        }
        s_sensor_online = is_online;
        xSemaphoreGive(s_lock);
    }
}

static esp_err_t metrics_get_handler(httpd_req_t *req)
{
    tvoc301_reading_t reading = {0};
    bool online = false;

    if (s_lock != NULL && xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        reading = s_latest_reading;
        online = s_sensor_online;
        xSemaphoreGive(s_lock);
    }

    double tvoc_val = (double)reading.tvoc / 1000.0;
    double ch2o_val = (double)reading.ch2o / 1000.0;
    double co2_val  = (double)reading.co2  / 1000.0;

    char resp_str[768];
    int len = snprintf(resp_str, sizeof(resp_str),
        "# HELP baby_tvoc_ug_m3 TVOC concentration in ug/m3\n"
        "# TYPE baby_tvoc_ug_m3 gauge\n"
        "baby_tvoc_ug_m3 %.3f\n\n"
        "# HELP baby_ch2o_mg_m3 Formaldehyde concentration in mg/m3\n"
        "# TYPE baby_ch2o_mg_m3 gauge\n"
        "baby_ch2o_mg_m3 %.3f\n\n"
        "# HELP baby_co2_ppm Carbon dioxide concentration in ppm\n"
        "# TYPE baby_co2_ppm gauge\n"
        "baby_co2_ppm %.3f\n\n"
        "# HELP baby_sensor_online Sensor online status (1=OK, 0=Error)\n"
        "# TYPE baby_sensor_online gauge\n"
        "baby_sensor_online %d\n",
        tvoc_val, ch2o_val, co2_val, online ? 1 : 0);

    httpd_resp_set_type(req, "text/plain; version=0.0.4; charset=utf-8");
    return httpd_resp_send(req, resp_str, len);
}

esp_err_t prometheus_server_start(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 5000;
    config.max_uri_handlers = 8;

    ESP_LOGI(TAG, "Starting Prometheus HTTP server on port %d...", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error starting HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t metrics_uri = {
        .uri       = "/metrics",
        .method    = HTTP_GET,
        .handler   = metrics_get_handler,
        .user_ctx  = NULL
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &metrics_uri));
    ESP_LOGI(TAG, "Prometheus metrics endpoint registered at /metrics");

    return ESP_OK;
}
