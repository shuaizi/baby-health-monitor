/* Compile the production driver with allocator hooks; leave this harness's
 * standard library calls untouched. Each test runs in a fresh process. */
#undef calloc
#undef free

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "ssd1306.h"

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);      \
            exit(1);                                                            \
        }                                                                       \
    } while (0)

static const ssd1306_config_t config = {
    .sda_pin = 8,
    .scl_pin = 9,
    .i2c_address = 0x3c,
    .i2c_clock_hz = 400000,
};
static int bus_token;
static int device_token;
static esp_err_t bus_result = ESP_OK;
static esp_err_t probe_result = ESP_OK;
static esp_err_t add_result = ESP_OK;
static esp_err_t transmit_result = ESP_OK;
static esp_err_t remove_result = ESP_OK;
static esp_err_t delete_bus_result = ESP_OK;
static int fail_transmit_call;
static int transmit_calls;
static int live_allocations;
static bool fail_allocation;
static bool live_bus;
static bool live_device;
static bool saw_drawn_pixels;
static char calls[128];
static size_t call_count;

static void record_call(char call)
{
    CHECK(call_count + 1 < sizeof(calls));
    calls[call_count++] = call;
    calls[call_count] = '\0';
}

void *test_calloc(size_t count, size_t size)
{
    if (fail_allocation) {
        return NULL;
    }
    void *allocation = calloc(count, size);
    CHECK(allocation != NULL);
    live_allocations++;
    return allocation;
}

void test_free(void *allocation)
{
    if (allocation != NULL) {
        CHECK(live_allocations > 0);
        live_allocations--;
    }
    free(allocation);
}

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *actual,
                           i2c_master_bus_handle_t *bus)
{
    record_call('C');
    CHECK(actual->i2c_port == I2C_NUM_0);
    CHECK(actual->sda_io_num == config.sda_pin);
    CHECK(actual->scl_io_num == config.scl_pin);
    CHECK(!live_bus);
    if (bus_result == ESP_OK) {
        *bus = &bus_token;
        live_bus = true;
    }
    return bus_result;
}

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address,
                         int timeout_ms)
{
    record_call('P');
    CHECK(live_bus && bus == &bus_token);
    CHECK(!live_device);
    CHECK(address == config.i2c_address);
    CHECK(timeout_ms == 100);
    return probe_result;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                  const i2c_device_config_t *actual,
                                  i2c_master_dev_handle_t *device)
{
    record_call('A');
    CHECK(live_bus && bus == &bus_token);
    CHECK(!live_device);
    CHECK(actual->dev_addr_length == I2C_ADDR_BIT_LEN_7);
    CHECK(actual->device_address == config.i2c_address);
    CHECK(actual->scl_speed_hz == config.i2c_clock_hz);
    if (add_result == ESP_OK) {
        *device = &device_token;
        live_device = true;
    }
    return add_result;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device, const uint8_t *data,
                            size_t length, int timeout_ms)
{
    record_call('T');
    CHECK(live_bus && live_device && device == &device_token);
    CHECK(data != NULL && length > 1);
    CHECK(timeout_ms > 0);
    CHECK(data[0] == 0x00 || data[0] == 0x40);
    if (data[0] == 0x40) {
        CHECK(length == 129);
        for (size_t index = 1; index < length; index++) {
            saw_drawn_pixels |= data[index] != 0;
        }
    }
    transmit_calls++;
    return transmit_calls == fail_transmit_call ? transmit_result : ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t device)
{
    record_call('R');
    CHECK(live_bus && live_device && device == &device_token);
    if (remove_result == ESP_OK) {
        live_device = false;
    }
    return remove_result;
}

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus)
{
    record_call('D');
    CHECK(live_bus && bus == &bus_token);
    CHECK(!live_device);
    if (delete_bus_result == ESP_OK) {
        live_bus = false;
    }
    return delete_bus_result;
}

static void check_released(void)
{
    CHECK(!live_bus);
    CHECK(!live_device);
    CHECK(live_allocations == 0);
}

static void test_invalid_arguments(void)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    CHECK(ssd1306_init(NULL, &config) == ESP_ERR_INVALID_ARG);
    CHECK(ssd1306_init(&device, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(device == NULL);
    CHECK(ssd1306_init(NULL, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(strcmp(calls, "") == 0);
    check_released();
}

static void test_allocation_failure(void)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    fail_allocation = true;
    CHECK(ssd1306_init(&device, &config) == ESP_ERR_NO_MEM);
    CHECK(device == NULL);
    CHECK(strcmp(calls, "") == 0);
    check_released();
}

static void test_bus_failure(void)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    bus_result = ESP_ERR_INVALID_STATE;
    CHECK(ssd1306_init(&device, &config) == ESP_ERR_INVALID_STATE);
    CHECK(device == NULL);
    CHECK(strcmp(calls, "C") == 0);
    check_released();
}

static void test_probe_failure(esp_err_t error)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    probe_result = error;
    CHECK(ssd1306_init(&device, &config) == error);
    CHECK(device == NULL);
    /* No add-device or transmit call may follow a failed probe. */
    CHECK(strcmp(calls, "CPD") == 0);
    check_released();
}

static void test_add_failure(void)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    add_result = ESP_ERR_NO_MEM;
    CHECK(ssd1306_init(&device, &config) == ESP_ERR_NO_MEM);
    CHECK(device == NULL);
    CHECK(strcmp(calls, "CPAD") == 0);
    check_released();
}

static void test_initial_write_failure(void)
{
    ssd1306_device_t *device = (ssd1306_device_t *)(uintptr_t)1;
    fail_transmit_call = 1;
    transmit_result = ESP_FAIL;
    CHECK(ssd1306_init(&device, &config) == ESP_FAIL);
    CHECK(device == NULL);
    CHECK(strcmp(calls, "CPATRD") == 0);
    check_released();
}

static void test_initial_write_cleanup_retry(void)
{
    ssd1306_device_t *device = NULL;
    fail_transmit_call = 1;
    transmit_result = ESP_ERR_TIMEOUT;
    remove_result = ESP_ERR_INVALID_STATE;
    /* The initial error wins over the cleanup error, but the retained handle
     * allows the caller to finish cleanup without leaking either resource. */
    CHECK(ssd1306_init(&device, &config) == ESP_ERR_TIMEOUT);
    CHECK(device != NULL);
    CHECK(strcmp(calls, "CPATR") == 0);
    CHECK(live_allocations == 1 && live_bus && live_device);
    remove_result = ESP_OK;
    CHECK(ssd1306_deinit(device) == ESP_OK);
    CHECK(strcmp(calls, "CPATRRD") == 0);
    check_released();
}

static void test_probe_cleanup_retry(void)
{
    ssd1306_device_t *device = NULL;
    probe_result = ESP_ERR_NOT_FOUND;
    delete_bus_result = ESP_ERR_INVALID_STATE;
    CHECK(ssd1306_init(&device, &config) == ESP_ERR_NOT_FOUND);
    CHECK(device != NULL);
    CHECK(strcmp(calls, "CPD") == 0);
    CHECK(live_allocations == 1 && live_bus && !live_device);
    delete_bus_result = ESP_OK;
    CHECK(ssd1306_deinit(device) == ESP_OK);
    CHECK(strcmp(calls, "CPDD") == 0);
    check_released();
}

static void test_success_and_release(void)
{
    ssd1306_device_t *device = NULL;
    CHECK(ssd1306_init(&device, &config) == ESP_OK);
    CHECK(device != NULL);
    CHECK(strcmp(calls, "CPAT") == 0);
    CHECK(live_allocations == 1 && live_bus && live_device);
    CHECK(ssd1306_deinit(device) == ESP_OK);
    CHECK(strcmp(calls, "CPATRD") == 0);
    check_released();
}

static void test_null_release(void)
{
    CHECK(ssd1306_deinit(NULL) == ESP_OK);
    CHECK(strcmp(calls, "") == 0);
    check_released();
}

static void test_release_device_retry(void)
{
    ssd1306_device_t *device = NULL;
    CHECK(ssd1306_init(&device, &config) == ESP_OK);
    remove_result = ESP_ERR_INVALID_STATE;
    CHECK(ssd1306_deinit(device) == ESP_ERR_INVALID_STATE);
    CHECK(strcmp(calls, "CPATR") == 0);
    CHECK(live_allocations == 1 && live_bus && live_device);
    remove_result = ESP_OK;
    CHECK(ssd1306_deinit(device) == ESP_OK);
    CHECK(strcmp(calls, "CPATRRD") == 0);
    check_released();
}

static void test_release_bus_retry(void)
{
    ssd1306_device_t *device = NULL;
    CHECK(ssd1306_init(&device, &config) == ESP_OK);
    delete_bus_result = ESP_ERR_INVALID_STATE;
    CHECK(ssd1306_deinit(device) == ESP_ERR_INVALID_STATE);
    CHECK(strcmp(calls, "CPATRD") == 0);
    CHECK(live_allocations == 1 && live_bus && !live_device);
    delete_bus_result = ESP_OK;
    CHECK(ssd1306_deinit(device) == ESP_OK);
    CHECK(strcmp(calls, "CPATRDD") == 0);
    check_released();
}

static void test_show_success(void)
{
    ssd1306_device_t *device = NULL;
    CHECK(ssd1306_init(&device, &config) == ESP_OK);
    ssd1306_clear(device);
    ssd1306_draw_text(device, 0, 0, "TVOC: 1.23");
    CHECK(ssd1306_show(device) == ESP_OK);
    CHECK(transmit_calls == 9); /* init + four command/data page pairs */
    CHECK(saw_drawn_pixels);
    CHECK(ssd1306_deinit(device) == ESP_OK);
    check_released();
}

static void test_show_failure(int failing_call)
{
    ssd1306_device_t *device = NULL;
    CHECK(ssd1306_init(&device, &config) == ESP_OK);
    fail_transmit_call = failing_call;
    transmit_result = ESP_ERR_TIMEOUT;
    CHECK(ssd1306_show(device) == ESP_ERR_TIMEOUT);
    CHECK(transmit_calls == failing_call);
    CHECK(ssd1306_deinit(device) == ESP_OK);
    check_released();
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (strcmp(argv[1], "invalid_arguments") == 0) {
        test_invalid_arguments();
    } else if (strcmp(argv[1], "allocation_failure") == 0) {
        test_allocation_failure();
    } else if (strcmp(argv[1], "bus_failure") == 0) {
        test_bus_failure();
    } else if (strcmp(argv[1], "missing_display_nack") == 0) {
        test_probe_failure(ESP_ERR_NOT_FOUND);
    } else if (strcmp(argv[1], "probe_timeout") == 0) {
        test_probe_failure(ESP_ERR_TIMEOUT);
    } else if (strcmp(argv[1], "probe_other_error") == 0) {
        test_probe_failure(ESP_FAIL);
    } else if (strcmp(argv[1], "add_failure") == 0) {
        test_add_failure();
    } else if (strcmp(argv[1], "initial_write_failure") == 0) {
        test_initial_write_failure();
    } else if (strcmp(argv[1], "initial_write_cleanup_retry") == 0) {
        test_initial_write_cleanup_retry();
    } else if (strcmp(argv[1], "probe_cleanup_retry") == 0) {
        test_probe_cleanup_retry();
    } else if (strcmp(argv[1], "success_and_release") == 0) {
        test_success_and_release();
    } else if (strcmp(argv[1], "null_release") == 0) {
        test_null_release();
    } else if (strcmp(argv[1], "release_device_retry") == 0) {
        test_release_device_retry();
    } else if (strcmp(argv[1], "release_bus_retry") == 0) {
        test_release_bus_retry();
    } else if (strcmp(argv[1], "show_success") == 0) {
        test_show_success();
    } else if (strcmp(argv[1], "show_command_failure") == 0) {
        test_show_failure(2);
    } else if (strcmp(argv[1], "show_data_failure") == 0) {
        test_show_failure(3);
    } else {
        fprintf(stderr, "Unknown test: %s\n", argv[1]);
        return 2;
    }
    printf("PASS %s\n", argv[1]);
    return 0;
}
