#pragma once

#include <cstdio>

#ifndef ESP_LOGI
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGD
#define ESP_LOGD(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGW
#define ESP_LOGW(tag, fmt, ...) printf("[%s] WARNING: " fmt "\n", tag, ##__VA_ARGS__)
#endif

#ifndef ESP_LOGE
#define ESP_LOGE(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
#endif
