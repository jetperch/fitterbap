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

#ifndef FBP_PY_CONFIG_H_
#define FBP_PY_CONFIG_H_


#define FBP_CONFIG_COMM_FRAMER_CRC32(data, length)  (0U)  // fbp_crc32(0, (data), (length))

/* Set global log level */
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL

/* Set the initial comm log port level for forwarding/receiving log messages. */
#define FBP_LOGP_LEVEL FBP_LOG_LEVEL_ALL

/* Override the log format */
void fbp_log_printf_(const char *fmt, ...);
#define FBP_LOG_PRINTF(level, format, ...) \
   fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);

// todo linux & mac support
#if defined(WIN32) || defined(_WIN32)
#include "fitterbap/host/win/config.h"
#else
#error "unsupported platform"
#endif

#define FBP_CONFIG_USE_FLOAT32 1
#define FBP_CONFIG_USE_FLOAT64 1
#define FBP_CONFIG_USE_CSTR_FLOAT 1

#endif /* FBP_PY_CONFIG_H_ */

