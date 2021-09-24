/*
 * Copyright 2014-2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 *
 * @brief FBP configuration.
 */

#ifndef FBP_CONFIG_H_
#define FBP_CONFIG_H_

/**
 * @ingroup fbp
 * @defgroup fbp_config Configuration
 *
 * @brief FBP configuration.
 *
 * @{
 */


/* Set global log level */
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL

/* Set the initial comm log port level for forwarding/receiving log messages. */
// #define FBP_LOGP_LEVEL FBP_LOG_LEVEL_WARNING

/* Optionally Override the log format */
#if 0  // use the included Fitterbap log handler
struct fbp_logh_s;
int32_t fbp_logh_publish(struct fbp_logh_s * self, uint8_t level, const char * filename, uint32_t line, const char * format, ...);
#define FBP_LOG_PRINTF(level, format, ...) \
    fbp_logh_publish(NULL, level, __FILENAME__, __LINE__, format, __VA_ARGS__)
#elif 0  // redefine the printf format
#define FBP_LOG_PRINTF(level, format, ...) \
   fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif

/**
 * @brief The 32-bit CRC function to use for the comm framer.
 *
 * The signature must be:
 *   uint32_t (*fn)(uint32_t crc, uint8_t const *data, uint32_t length)
 */
//#define FBP_CONFIG_COMM_FRAMER_CRC32 fbp_crc32
//#define FBP_CRC_CRC32 1


// Uncomment for your platform
//#define FBP_CONFIG_USE_PLATFORM_STDLIB 1
//#define FBP_PLATFORM_ARM 1

// remove the following for custom platforms
#ifdef __linux__
#include "fitterbap/host/linux/config.h"
#elif _WIN32
#include "fitterbap/host/win/config.h"
#else
#endif

// 1 to enable floating point
// #define FBP_CONFIG_USE_CSTR_FLOAT 0

// typedef void * fbp_os_mutex_t;


/** @} */

#endif /* FBP_CONFIG_H_ */

