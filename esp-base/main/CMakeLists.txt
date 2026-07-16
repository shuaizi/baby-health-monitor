idf_component_register(SRCS "hello_world_main.c" "ssd1306.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c spi_flash
                       INCLUDE_DIRS "")
