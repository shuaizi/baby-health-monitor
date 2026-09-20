| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# Baby Health Monitor

ESP32-S3 firmware that reads a TVOC301 sensor over UART, optionally displays
readings on an SSD1306 OLED, and exposes Prometheus metrics at `/metrics` over
Wi-Fi. The current configuration is tested with ESP-IDF 6.0.2.

(See the README.md file in the upper level 'examples' directory for more information about examples.)

## How to use example

Follow detailed instructions provided specifically for this example.

### Local Wi-Fi configuration

From `esp-base`, activate ESP-IDF and run:

```sh
idf.py menuconfig
idf.py build
# When ready to program the board:
idf.py -p YOUR_SERIAL_PORT flash
```

In **Baby Health Monitor**, set **Wi-Fi SSID (2.4 GHz)** and **Wi-Fi password**.
The SSID and password are read through `CONFIG_BABY_HEALTH_WIFI_SSID` and
`CONFIG_BABY_HEALTH_WIFI_PASSWORD`; there are no network credentials in C source.
An empty SSID skips Wi-Fi and the HTTP server, but sensor and display processing
continue. An empty password is supported for an open network.

The repository tracks only `sdkconfig.defaults`, which keeps public settings
and empty Wi-Fi defaults. The generated local `sdkconfig`, its `sdkconfig.old`
backup, and `build/` are ignored. Keep your existing local `sdkconfig` when
updating the project so your network settings are preserved.

**Credential safety:** `sdkconfig` and firmware binaries still contain the
credentials in plaintext; ignoring files prevents Git disclosure, not device
extraction. Do not share your local configuration or firmware. Running
`idf.py save-defconfig` after entering credentials can copy them into
`sdkconfig.defaults`: reset both Wi-Fi values to empty before committing that
file. After `idf.py fullclean` the local `sdkconfig` is retained, but operations
that recreate/reset it require checking or entering credentials again.

### Optional SSD1306 display

The OLED is optional. By default, startup probes the configured I2C address
(`0x3C`, SDA GPIO8, SCL GPIO9) with a 100 ms timeout before sending any display
commands. An ACK only indicates a device at that address; it does not identify
the controller model.

- If no device responds or initialization fails, display output is disabled
  for that boot and I2C resource cleanup is attempted. A cleanup failure is
  logged without re-enabling display output. TVOC301 sampling,
  LED status, Wi-Fi and the Prometheus `/metrics` endpoint continue normally.
- If an update fails after startup, further rendering and display writes are
  disabled rather than repeatedly retrying on every sample.
- Reconnect the display and restart the board to enable it again; hot-plug
  auto-recovery is not implemented.

To disable the display explicitly, run `idf.py menuconfig`, open **Baby Health
Monitor**, and turn off **Enable SSD1306 display (auto-detect on startup)**
(`CONFIG_BABY_HEALTH_DISPLAY_ENABLED`). This also skips I2C initialization and
the startup probe. Rebuild and flash to apply a configuration change.
For an existing build directory created before this option was added, run
`idf.py reconfigure` once so the new configuration option is discovered.

Select the instructions depending on Espressif chip installed on your development board:

- [ESP32 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)
- [ESP32-S2 Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html)


## Example folder contents

The project **esp-base** starts in [hello_world_main.c](main/hello_world_main.c).
The other files under [main](main) implement the sensor, display, Wi-Fi and
Prometheus endpoint.

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt` files that provide set of directives and instructions describing the project's source files and targets (executable, library, or both).

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── sdkconfig.defaults         Public, credential-free project configuration
├── pytest_hello_world.py      Python script used for automated testing
├── main
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   ├── hello_world_main.c
│   └── ...                    Peripheral and network modules
├── tests                      Host regression tests with mocked hardware
└── README.md                  This is the file you are currently reading
```

For more information on structure and contents of ESP-IDF projects, please refer to Section [Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html) of the ESP-IDF Programming Guide.

## Host regression tests

From the repository root, run:

```sh
python3 -m unittest discover -s esp-base/tests -p 'test_*.py' -v
```

These tests compile the production C driver and display state helpers with
mock hardware interfaces. They cover display detection, cleanup, the disabled
configuration, and suppression of rendering after a failure. Only Python's
standard library and a host C compiler (`cc`, or the `CC` environment variable)
are required. They do not verify physical wiring or replace a hardware test.

## Troubleshooting

* Program upload failure

    * Hardware connection is not correct: run `idf.py -p PORT monitor`, and reboot your board to see if there are any output logs.
    * The baud rate for downloading is too high: lower your baud rate in the `menuconfig` menu, and try again.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-idf/issues)

We will get back to you as soon as possible.
