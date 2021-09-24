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


/* Set global log level */
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL

/* Set the initial comm log port level for forwarding/receiving log messages. */
#define FBP_LOGP_LEVEL FBP_LOG_LEVEL_ALL


/* Override the log format */
/*
#define FBP_LOG_PRINTF(level, format, ...) \
   fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif
*/

#ifdef __linux__
#include "fitterbap/host/linux/config.h"
#elif _WIN32
#include "fitterbap/host/win/config.h"
#else
#endif


#endif /* FBP_CONFIG_H_ */

