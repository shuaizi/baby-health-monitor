#pragma once

static inline void test_log_warning(const char *tag, const char *format, ...)
{
    (void)tag;
    (void)format;
}

#define ESP_LOGW(tag, ...) test_log_warning(tag, __VA_ARGS__)
