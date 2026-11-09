#pragma once

#include <Arduino.h>

// HMI 진단 로그는 USB CDC host 연결을 기다리지 않고 즉시 반환한다.
#define HMI_LOG_LEVEL_OFF 0
#define HMI_LOG_LEVEL_ERROR 1
#define HMI_LOG_LEVEL_INFO 2
#define HMI_LOG_LEVEL_DEBUG 3

#ifndef HMI_LOG_LEVEL
#define HMI_LOG_LEVEL HMI_LOG_LEVEL_INFO
#endif

#if HMI_LOG_LEVEL > HMI_LOG_LEVEL_OFF
#define HMI_LOG_BEGIN(baud)             \
  do {                                  \
    Serial.begin(baud);                 \
    Serial.setTxTimeoutMs(0);           \
  } while (0)
#else
#define HMI_LOG_BEGIN(baud) \
  do {                      \
    (void)(baud);           \
  } while (0)
#endif

#define HMI_LOG_WRITE_IF_CONNECTED(...) \
  do {                                  \
    if (Serial) {                        \
      Serial.printf(__VA_ARGS__);        \
    }                                   \
  } while (0)

#if HMI_LOG_LEVEL >= HMI_LOG_LEVEL_ERROR
#define HMI_LOG_ERROR(...) HMI_LOG_WRITE_IF_CONNECTED(__VA_ARGS__)
#else
#define HMI_LOG_ERROR(...) \
  do {                     \
  } while (0)
#endif

#if HMI_LOG_LEVEL >= HMI_LOG_LEVEL_INFO
#define HMI_LOG_INFO(...) HMI_LOG_WRITE_IF_CONNECTED(__VA_ARGS__)
#else
#define HMI_LOG_INFO(...) \
  do {                    \
  } while (0)
#endif

#if HMI_LOG_LEVEL >= HMI_LOG_LEVEL_DEBUG
#define HMI_LOG_DEBUG(...) HMI_LOG_WRITE_IF_CONNECTED(__VA_ARGS__)
#else
#define HMI_LOG_DEBUG(...) \
  do {                     \
  } while (0)
#endif
