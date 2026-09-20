/* Exercise real static helpers without starting app_main, hardware, or networking. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../main/hello_world_main.c"

struct ssd1306_device {
    int placeholder;
};

static struct ssd1306_device mock_device;
static esp_err_t init_result = ESP_OK;
static esp_err_t show_result = ESP_OK;
static esp_err_t deinit_result = ESP_OK;
static esp_err_t wifi_result = ESP_OK;
static esp_err_t http_result = ESP_OK;
static unsigned wifi_calls;
static unsigned http_calls;

typedef struct {
    unsigned init;
    unsigned clear;
    unsigned draw;
    unsigned show;
    unsigned deinit;
} display_calls_t;

static display_calls_t calls;
static char lines[3][32];
static const tvoc301_reading_t sample = { .tvoc = 1234, .ch2o = 5678, .co2 = 42001 };

esp_err_t ssd1306_init(ssd1306_device_t **device, const ssd1306_config_t *config)
{
    ++calls.init;
    assert(*device == NULL);
    assert(config->sda_pin == GPIO_NUM_8);
    assert(config->scl_pin == GPIO_NUM_9);
    assert(config->i2c_address == 0x3c);
    assert(config->i2c_clock_hz == 400000);
    *device = init_result == ESP_OK ? &mock_device : NULL;
    return init_result;
}

esp_err_t ssd1306_deinit(ssd1306_device_t *device)
{
    assert(device == &mock_device);
    ++calls.deinit;
    return deinit_result;
}

void ssd1306_clear(ssd1306_device_t *device)
{
    assert(device == &mock_device);
    ++calls.clear;
    memset(lines, 0, sizeof(lines));
}

void ssd1306_draw_text(ssd1306_device_t *device, uint8_t x, uint8_t y, const char *text)
{
    assert(device == &mock_device);
    assert(x == 0);
    assert(y == 0 || y == 12 || y == 24);
    assert(strlen(text) < sizeof(lines[0]));
    ++calls.draw;
    strcpy(lines[y / 12], text);
}

esp_err_t ssd1306_show(ssd1306_device_t *device)
{
    assert(device == &mock_device);
    ++calls.show;
    return show_result;
}

const char *esp_err_to_name(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return "ESP_OK";
    case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    default: return "ESP_FAIL";
    }
}

/* These satisfy the compiled production entry point, but must never execute. */
esp_err_t gpio_config(const gpio_config_t *config) { (void)config; abort(); }
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) { (void)pin; (void)level; abort(); }
void esp_chip_info(esp_chip_info_t *info) { (void)info; abort(); }
esp_err_t esp_flash_get_size(void *chip, uint32_t *size) { (void)chip; (void)size; abort(); }
uint32_t esp_get_minimum_free_heap_size(void) { abort(); }
void vTaskDelay(TickType_t ticks) { (void)ticks; abort(); }
esp_err_t tvoc301_init(const tvoc301_config_t *config) { (void)config; abort(); }
esp_err_t tvoc301_read(tvoc301_reading_t *reading, uint32_t timeout_ms)
{ (void)reading; (void)timeout_ms; abort(); }
void prometheus_server_update_reading(const tvoc301_reading_t *reading, bool online)
{ (void)reading; (void)online; abort(); }

esp_err_t wifi_app_init_sta(const char *ssid, const char *password)
{
    assert(TEST_WIFI_CONFIGURED);
    assert(strcmp(ssid, "test-network") == 0);
    assert(strcmp(password, "test-password") == 0);
    assert(http_calls == 0);
    ++wifi_calls;
    return wifi_result;
}

esp_err_t prometheus_server_start(void)
{
    assert(wifi_calls == 1);
    assert(wifi_result == ESP_OK);
    ++http_calls;
    return http_result;
}

static void test_network_unconfigured(void)
{
    assert(!TEST_WIFI_CONFIGURED);
    init_network();
    assert(wifi_calls == 0);
    assert(http_calls == 0);
}

static void test_network_configured(esp_err_t wifi_error, esp_err_t http_error)
{
    assert(TEST_WIFI_CONFIGURED);
    wifi_result = wifi_error;
    http_result = http_error;
    init_network();
    assert(wifi_calls == 1);
    assert(http_calls == (wifi_error == ESP_OK ? 1U : 0U));
}

static void assert_no_more_output(display_state_t *display)
{
    assert(!display->enabled);
    const display_calls_t previous = calls;
    for (int i = 0; i < 3; ++i) {
        draw_reading(display, &sample);
        draw_status(display, "TVOC TIMEOUT", "CHECK UART", "RX GPIO18");
    }
    assert(calls.init == previous.init);
    assert(calls.clear == previous.clear);
    assert(calls.draw == previous.draw);
    assert(calls.show == previous.show);
    assert(calls.deinit == previous.deinit);
}

static void assert_successful_start(display_state_t *display)
{
    assert(display->enabled);
    assert(display->device == &mock_device);
    assert(calls.init == 1 && calls.clear == 1 && calls.draw == 3);
    assert(calls.show == 1 && calls.deinit == 0);
    assert(strcmp(lines[0], "BABY HEALTH") == 0);
    assert(strcmp(lines[1], "TVOC READY") == 0);
    assert(strcmp(lines[2], "WAIT DATA") == 0);
}

static void test_config_disabled(void)
{
    assert(!CONFIG_BABY_HEALTH_DISPLAY_ENABLED);
    display_state_t display = init_display();
    assert(display.device == NULL);
    assert(calls.init == 0 && calls.clear == 0 && calls.draw == 0);
    assert(calls.show == 0 && calls.deinit == 0);
    assert_no_more_output(&display);
}

static void test_initialization_error(esp_err_t error)
{
    init_result = error;
    display_state_t display = init_display();
    assert(display.device == NULL);
    assert(calls.init == 1 && calls.clear == 0 && calls.draw == 0);
    assert(calls.show == 0 && calls.deinit == 0);
    assert_no_more_output(&display);
}

static void test_normal_rendering(void)
{
    display_state_t display = init_display();
    assert_successful_start(&display);
    draw_reading(&display, &sample);
    assert(strcmp(lines[0], "TVOC:1.234") == 0);
    assert(strcmp(lines[1], "CH2O:5.678") == 0);
    assert(strcmp(lines[2], "CO2 :42.001") == 0);
    assert(calls.clear == 2 && calls.draw == 6 && calls.show == 2);
    draw_status(&display, "TVOC TIMEOUT", "CHECK UART", "RX GPIO18");
    assert(strcmp(lines[0], "TVOC TIMEOUT") == 0);
    assert(strcmp(lines[1], "CHECK UART") == 0);
    assert(strcmp(lines[2], "RX GPIO18") == 0);
    assert(calls.clear == 3 && calls.draw == 9 && calls.show == 3);
    assert(display.enabled && calls.deinit == 0);
    draw_reading(&display, &sample);
    assert(display.enabled && calls.show == 4 && calls.deinit == 0);
}

static void test_show_error(bool during_init, bool status_screen, bool cleanup_fails)
{
    if (during_init) {
        show_result = ESP_ERR_TIMEOUT;
    }
    deinit_result = cleanup_fails ? ESP_FAIL : ESP_OK;
    display_state_t display = init_display();
    if (!during_init) {
        assert_successful_start(&display);
        show_result = ESP_ERR_TIMEOUT;
        if (status_screen) {
            draw_status(&display, "TVOC TIMEOUT", "CHECK UART", "RX GPIO18");
        } else {
            draw_reading(&display, &sample);
        }
    }
    assert(!display.enabled);
    assert(display.device == (cleanup_fails ? &mock_device : NULL));
    assert(calls.show == (during_init ? 1U : 2U));
    assert(calls.deinit == 1);
    /* Even if the I2C errors clear, output and cleanup are not retried. */
    show_result = ESP_OK;
    deinit_result = ESP_OK;
    assert_no_more_output(&display);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *scenario = argv[1];
    if (strcmp(scenario, "network_unconfigured") == 0) {
        test_network_unconfigured();
    } else if (strcmp(scenario, "network_normal") == 0) {
        test_network_configured(ESP_OK, ESP_OK);
    } else if (strcmp(scenario, "network_wifi_error") == 0) {
        test_network_configured(ESP_FAIL, ESP_OK);
    } else if (strcmp(scenario, "network_http_error") == 0) {
        test_network_configured(ESP_OK, ESP_FAIL);
    } else if (strcmp(scenario, "disabled") == 0) {
        test_config_disabled();
    } else {
        assert(CONFIG_BABY_HEALTH_DISPLAY_ENABLED);
        if (strcmp(scenario, "missing") == 0) {
            test_initialization_error(ESP_ERR_NOT_FOUND);
        } else if (strcmp(scenario, "bus_error") == 0) {
            test_initialization_error(ESP_ERR_TIMEOUT);
        } else if (strcmp(scenario, "normal") == 0) {
            test_normal_rendering();
        } else if (strcmp(scenario, "initial_show_error") == 0) {
            test_show_error(true, false, false);
        } else if (strcmp(scenario, "reading_show_error") == 0) {
            test_show_error(false, false, false);
        } else if (strcmp(scenario, "status_show_error") == 0) {
            test_show_error(false, true, false);
        } else if (strcmp(scenario, "initial_cleanup_error") == 0) {
            test_show_error(true, false, true);
        } else if (strcmp(scenario, "reading_cleanup_error") == 0) {
            test_show_error(false, false, true);
        } else if (strcmp(scenario, "status_cleanup_error") == 0) {
            test_show_error(false, true, true);
        } else {
            assert(!"unknown test scenario");
        }
    }
    return 0;
}
