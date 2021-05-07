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
 * @brief Define "missing" inttypes.
 */

#ifndef FBP_INTTYPES_H_
#define FBP_INTTYPES_H_

#include <inttypes.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_inttypes inttypes
 *
 * @brief Define "missing" inttypes
 *
 * This c99 omission makes me sad.
 *
 * @{
 */

#if defined(__LP64__) || defined(_LP64)
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX
#endif

#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"

/** @} */

#endif /* FBP_INTTYPES_H_ */
