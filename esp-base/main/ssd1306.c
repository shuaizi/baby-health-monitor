#include "ssd1306.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32
#define SSD1306_PAGE_COUNT (SSD1306_HEIGHT / 8)
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_PAGE_COUNT)
#define SSD1306_COMMAND_PREFIX 0x00
#define SSD1306_DATA_PREFIX 0x40
#define SSD1306_TRANSFER_TIMEOUT_MS 1000

struct ssd1306_device {
    i2c_master_dev_handle_t i2c_device;
    uint8_t framebuffer[SSD1306_BUFFER_SIZE];
};

static esp_err_t send_commands(ssd1306_device_t *device, const uint8_t *commands, size_t length)
{
    uint8_t packet[32] = {SSD1306_COMMAND_PREFIX};

    if (length > sizeof(packet) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(&packet[1], commands, length);
    return i2c_master_transmit(device->i2c_device, packet, length + 1, SSD1306_TRANSFER_TIMEOUT_MS);
}

static const uint8_t *glyph_for(char character)
{
    static const uint8_t blank[] = {0, 0, 0, 0, 0};
    static const uint8_t digits[][5] = {
        {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0x00, 0x42, 0x7f, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31},
        {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e},
    };
    static const uint8_t letters[][5] = {
        {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
        {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
        {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
        {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
        {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01},
        {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
        {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
        {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
        {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
        {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
        {0x7f, 0x20, 0x18, 0x20, 0x7f}, {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43},
    };
    static const uint8_t colon[] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t dash[] = {0x08, 0x08, 0x08, 0x08, 0x08};

    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }
    if (character >= 'A' && character <= 'Z') {
        return letters[character - 'A'];
    }
    if (character == ':') {
        return colon;
    }
    if (character == '-') {
        return dash;
    }
    return blank;
}

static void set_pixel(ssd1306_device_t *device, uint8_t x, uint8_t y)
{
    if (x < SSD1306_WIDTH && y < SSD1306_HEIGHT) {
        device->framebuffer[x + (y / 8) * SSD1306_WIDTH] |= 1U << (y % 8);
    }
}

esp_err_t ssd1306_init(ssd1306_device_t **device, const ssd1306_config_t *config)
{
    static const uint8_t init_commands[] = {
        0xae, 0x20, 0x02, 0x40, 0x81, 0x7f, 0xa1, 0xa6, 0xa8, 0x1f,
        0xc8, 0xd3, 0x00, 0xd5, 0x80, 0xd9, 0xf1, 0xda, 0x02, 0xdb,
        0x40, 0x8d, 0x14, 0xaf,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_address,
        .scl_speed_hz = config->i2c_clock_hz,
    };
    ssd1306_device_t *new_device = calloc(1, sizeof(*new_device));
    esp_err_t result;

    if (new_device == NULL) {
        return ESP_ERR_NO_MEM;
    }
    result = i2c_new_master_bus(&bus_config, &bus_handle);
    if (result == ESP_OK) {
        result = i2c_master_bus_add_device(bus_handle, &device_config, &new_device->i2c_device);
    }
    if (result == ESP_OK) {
        result = send_commands(new_device, init_commands, sizeof(init_commands));
    }
    if (result != ESP_OK) {
        free(new_device);
        return result;
    }

    *device = new_device;
    return ESP_OK;
}

void ssd1306_clear(ssd1306_device_t *device)
{
    memset(device->framebuffer, 0, sizeof(device->framebuffer));
}

void ssd1306_draw_text(ssd1306_device_t *device, uint8_t x, uint8_t y, const char *text)
{
    while (*text != '\0' && x <= SSD1306_WIDTH - 5) {
        const uint8_t *glyph = glyph_for(*text++);
        for (uint8_t column = 0; column < 5; column++) {
            for (uint8_t row = 0; row < 7; row++) {
                if (glyph[column] & (1U << row)) {
                    set_pixel(device, x + column, y + row);
                }
            }
        }
        x += 6;
    }
}

esp_err_t ssd1306_show(ssd1306_device_t *device)
{
    for (uint8_t page = 0; page < SSD1306_PAGE_COUNT; page++) {
        const uint8_t page_address[] = {0xb0 | page, 0x00, 0x10};
        uint8_t packet[SSD1306_WIDTH + 1] = {SSD1306_DATA_PREFIX};
        esp_err_t result = send_commands(device, page_address, sizeof(page_address));

        if (result != ESP_OK) {
            return result;
        }
        memcpy(&packet[1], &device->framebuffer[page * SSD1306_WIDTH], SSD1306_WIDTH);
        result = i2c_master_transmit(device->i2c_device, packet, sizeof(packet), SSD1306_TRANSFER_TIMEOUT_MS);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}
