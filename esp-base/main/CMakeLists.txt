idf_component_register(SRCS "hello_world_main.c" "ssd1306.c" "tvoc301.c" "wifi_app.c" "prometheus_server.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_uart spi_flash esp_wifi esp_event esp_netif nvs_flash esp_http_server
                       INCLUDE_DIRS ".")
