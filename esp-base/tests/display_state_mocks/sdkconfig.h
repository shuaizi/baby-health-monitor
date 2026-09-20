#pragma once

/* CONFIG_BABY_HEALTH_DISPLAY_ENABLED is set by the host test compiler. */
#define CONFIG_IDF_TARGET "host-test"

/* Fictional credentials only; never include the project's local sdkconfig. */
#if TEST_WIFI_CONFIGURED
#define CONFIG_BABY_HEALTH_WIFI_SSID "test-network"
#define CONFIG_BABY_HEALTH_WIFI_PASSWORD "test-password"
#else
#define CONFIG_BABY_HEALTH_WIFI_SSID ""
#define CONFIG_BABY_HEALTH_WIFI_PASSWORD ""
#endif
