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
 * @brief Assert
 */

#ifndef FBP_ASSERT_H_
#define FBP_ASSERT_H_

#include "fitterbap/platform.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_assert Assert
 *
 * @brief Assert and halt execution.
 *
 * This module provides assertion checking.  When an assertion fails,
 * program execution halts.  On an embedded system, the recommended behavior
 * is to log the fault and then reboot.
 *
 * @{
 */

/**
 * @brief Check a condition that is normally true.
 *
 * @param[in] condition The condition which is normally true.
 *      when false, invoke fault().
 */
#define FBP_ASSERT(condition) \
    if (! (condition) ) { fbp_fatal(__FILE__, __LINE__, "assert"); }

/**
 * @brief Check that a memory allocation succeeded (not NULL).
 *
 * @param[in] pointer The pointer, usually returned from malloc/calloc,
 *      which must not be NULL.
 */
#define FBP_ASSERT_ALLOC(pointer) \
    if (0 == (pointer)) { fbp_fatal(__FILE__, __LINE__, "memory allocation"); }

/**
 * @brief Signal that a fatal error occurred.
 *
 * @param[in] msg The message for debugging.
 */
#define FBP_FATAL(msg) fbp_fatal(__FILE__, __LINE__, msg)

/** @} */

#endif /* FBP_ASSERT_H_ */
