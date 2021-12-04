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
 * @brief The FBP shared library support functions.
 */

#ifndef FBP_CONFIG_DEFAULTS_H_
#define FBP_CONFIG_DEFAULTS_H_

#include "fitterbap/cmacro_inc.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_config config
 *
 * @brief The FBP configuration defaults.
 *
 * @{
 */

FBP_CPP_GUARD_START

#ifndef FBP_CONFIG_OS_MUTEX_LOCK_TIMEOUT_MS
#define FBP_CONFIG_OS_MUTEX_LOCK_TIMEOUT_MS  (500)
#endif

#ifndef FBP_CONFIG_COMM_FRAMER_CRC32
#define FBP_CONFIG_COMM_FRAMER_CRC32(data, length)  fbp_crc32(0, (data), (length))
#endif

#ifndef FBP_CONFIG_USE_CRC32
#define FBP_CONFIG_USE_CRC32 1
#endif

#ifndef FBP_CONFIG_USE_FLOAT32
#define FBP_CONFIG_USE_FLOAT32 0
#endif

#ifndef FBP_CONFIG_USE_FLOAT64
#define FBP_CONFIG_USE_FLOAT64 0
#endif

#ifndef FBP_CONFIG_USE_CSTR_FLOAT
#define FBP_CONFIG_USE_CSTR_FLOAT 0
#endif

// optional logging defines
// #define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL
// #define FBP_LOG_PRINTF(level, format, ...) my_printf("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);

// required typedefs
// typedef void * fbp_os_mutex_t;
// typedef intptr_t fbp_size_t;

#ifndef FBP_FATAL
#define FBP_FATAL(msg) fbp_fatal(__FILE__, __LINE__, msg)
#endif

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_CONFIG_DEFAULTS_H_ */
