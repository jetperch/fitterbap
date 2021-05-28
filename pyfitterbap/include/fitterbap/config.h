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

/* Override the log format */
/*
#define FBP_LOG_PRINTF(level, format, ...) \
   fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif
*/

#define FBP_FRAMER_CRC32 fbp_crc32
#define FBP_CRC_CRC32 1

#ifdef __linux__
#include "fitterbap/host/linux/config.h"
#elif _WIN32
#include "fitterbap/host/win/config.h"
#else
#endif

/** @} */

#endif /* FBP_CONFIG_H_ */

