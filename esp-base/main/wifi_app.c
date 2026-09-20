#include "wifi_app.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_APP";

static bool s_connected = false;
static char s_ip_str[16] = "0.0.0.0";
static int s_retry_num = 0;
#define MAXIMUM_RETRY 10

static void log_disconnect_reason(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
            ESP_LOGE(TAG, "Disconnect Reason %d: AP not found! Note: ESP32-S3 ONLY supports 2.4GHz Wi-Fi networks.", reason);
            break;
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_AUTH_LEAVE:
        case WIFI_REASON_ASSOC_NOT_AUTHED:
            ESP_LOGE(TAG, "Disconnect Reason %d: Authentication or security mode mismatch.", reason);
            break;
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            ESP_LOGE(TAG, "Disconnect Reason %d: Handshake timeout. Check Wi-Fi password or signal quality.", reason);
            break;
        default:
            ESP_LOGE(TAG, "Disconnect Reason code: %d", reason);
            break;
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to AP...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        s_connected = false;
        strncpy(s_ip_str, "0.0.0.0", sizeof(s_ip_str));

        log_disconnect_reason(event->reason);

        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to AP (%d/%d)...", s_retry_num, MAXIMUM_RETRY);
        } else {
            ESP_LOGW(TAG, "Failed to connect to AP after maximum retries");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        s_connected = true;
        ESP_LOGI(TAG, "Wi-Fi Connected! Got IP: %s", s_ip_str);
    }
}

esp_err_t wifi_app_init_sta(const char *ssid, const char *password)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    if (ssid != NULL) {
        strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    }
    if (password != NULL) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization STA finished for SSID: %s", ssid ? ssid : "");
    return ESP_OK;
}

bool wifi_app_is_connected(void)
{
    return s_connected;
}

void wifi_app_get_ip_string(char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0) return;
    strncpy(buf, s_ip_str, max_len - 1);
    buf[max_len - 1] = '\0';
}
