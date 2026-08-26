#pragma once

#include <cstdio>

#ifndef ESP_LOGI
#define ESP_LOGI(tag, ...)                                                     \
  do {                                                                         \
    printf("[%s] INFO: ", tag);                                                \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  } while (0)
#define ESP_LOGW(tag, ...)                                                     \
  do {                                                                         \
    printf("[%s] WARNING: ", tag);                                             \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  } while (0)
#define ESP_LOGE(tag, ...)                                                     \
  do {                                                                         \
    printf("[%s] ERROR: ", tag);                                               \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  } while (0)
#define ESP_LOGD(tag, ...)                                                     \
  do {                                                                         \
    printf("[%s] DEBUG: ", tag);                                               \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  } while (0)
#define ESP_LOGCONFIG(tag, ...)                                                \
  do {                                                                         \
    printf("[%s] CONFIG: ", tag);                                              \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  } while (0)
#endif
