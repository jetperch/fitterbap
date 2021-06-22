/*
 * Copyright 2021 Jetperch LLC
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
 * @brief Default linux host configuration.
 */

#ifndef FBP_HOST_LINUX_CONFIG_H__
#define FBP_HOST_LINUX_CONFIG_H__

#include "fitterbap/config.h"

#define FBP_FRAMER_CRC32 fbp_crc32
#define FBP_CRC_CRC32 1

#define FBP_PLATFORM_STDLIB 1
#define FBP_CSTR_FLOAT_ENABLE 0
typedef void * fbp_os_mutex_t;

#endif  /* FBP_HOST_LINUX_CONFIG_H__ */
