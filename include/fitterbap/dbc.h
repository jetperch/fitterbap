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
 * @brief Design by contract macros.
 */

#ifndef FBP_FBP_DBC_H_
#define FBP_FBP_DBC_H_

#include "log.h"
#include "fitterbap/assert.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_dbc Design by contract
 *
 * @brief Macros and functions to support design by contract.
 *
 * When a DBC check fails, these macros call fbp_fatal().  For most embedded
 * systems, this intentionally halts the program and reboots the system.
 * These checks should be used for internal APIs where error handling is
 * not meaningful.  For error handling see argchk.h.
 *
 * References include:
 *
 * - http://dbc.rubyforge.org/
 * - https://barrgroup.com/Embedded-Systems/How-To/Design-by-Contract-for-Embedded-Software
 *
 * @{
 */

/**
 * @brief Assert on a design-by-contract condition.
 *
 * @param condition The condition for the assert.  The condition is successful
 *      when true.  When false, an error has occurred.
 * @param message The message to display when condition is false.
 */
#define FBP_DBC_ASSERT(condition, message) do { \
    if (!(condition)) { \
        fbp_fatal(__FILENAME__, __LINE__, (message)); \
    } \
} while (0);

/**
 * @brief Check for a "true" value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_DBC_TRUE(x) FBP_DBC_ASSERT((x), #x " is false")

/**
 * @brief Check for a "false" value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_DBC_FALSE(x) FBP_DBC_ASSERT(!(x), #x " is true")

/**
 * @brief Assert on a null value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_DBC_NOT_NULL(x) FBP_DBC_ASSERT((x) != 0, #x " is null")

/**
 * @brief Assert that two values are strictly equal.
 *
 * @param a The first value.
 * @param b The second value.
 */
#define FBP_DBC_EQUAL(a, b) FBP_DBC_ASSERT((a) == (b), #a " != " #b)

/**
 * @brief Assert that a first value is greater than a second value.
 *
 * @param x The first value.
 * @param y The second value.
 */
#define FBP_DBC_GT(x, y) FBP_DBC_ASSERT((x) > (y), #x " !> " #y)

/**
 * @brief Assert that a first value is greater than or equal to a second value.
 *
 * @param x The first value.
 * @param y The second value.
 */
#define FBP_DBC_GTE(x, y) FBP_DBC_ASSERT((x) >= (y), #x " !>= " #y)

/**
 * @brief Assert that two values are not equal.
 *
 * @param x The first value.
 * @param y The second value.
 */
#define FBP_DBC_NE(x, y) FBP_DBC_ASSERT((x) != (y), #x " != " #y)

/**
 * @brief Assert that a first value is less than a second value.
 *
 * @param x The first value.
 * @param y The second value.
 */
#define FBP_DBC_LT(x, y) FBP_DBC_ASSERT((x) < (y), #x " !< " #y)

/**
 * @brief Assert that a first value is less than or equal to a second value.
 *
 * @param x The first value.
 * @param y The second value.
 */
#define FBP_DBC_LTE(x, y) FBP_DBC_ASSERT((x) <= (y), #x " !<= " #y)

/**
 * @brief Assert that a value is greater than zero.
 *
 * @param x The function argument to check.
 */
#define FBP_DBC_GT_ZERO(x) FBP_DBC_GT(x, 0)

/**
 * @brief Assert that a value is greater than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_DBC_GTE_ZERO(x) FBP_DBC_GTE(x, 0)

/**
 * @brief Assert that a value is not equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_DBC_NE_ZERO(x) FBP_DBC_NE(x, 0)

/**
 * @brief Assert that a value is less than zero.
 *
 * @param x The function argument to check.
 */
#define FBP_DBC_LT_ZERO(x) FBP_DBC_LT(x, 0)

/**
 * @brief Assert that a value is less than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_DBC_LTE_ZERO(x) FBP_DBC_LTE(x, 0)

/**
 * @brief Assert that a value is within a range.
 *
 * @param x The function argument to check.
 * @param x_min The minimum value, inclusive.
 * @param x_max The maximum value, inclusive.
 */
#define FBP_DBC_RANGE_INT(x, x_min, x_max)  do { \
    int x__ = (x); \
    int x_min__ = (x_min); \
    int x_max__ = (x_max); \
    FBP_DBC_ASSERT(x__ >= x_min__, #x " too small"); \
    FBP_DBC_ASSERT(x__ <= x_max__, #x " too big"); \
} while(0)

/**
 * @brief Assert that a value is within a range.
 *
 * @param type The type for the argumnet
 * @param x The function argument to check.
 * @param x_min The minimum value, inclusive.
 * @param x_max The maximum value, inclusive.
 */
#define FBP_DBC_RANGE_TYPE(type, x, x_min, x_max)  do { \
    type x__ = (x); \
    type x_min__ = (x_min); \
    type x_max__ = (x_max); \
    FBP_DBC_ASSERT(x__ >= x_min__, #x " too small"); \
    FBP_DBC_ASSERT(x__ <= x_max__, #x " too big"); \
} while(0)

/**
 * @brief Assert on a value.
 *
 * @param x The function argument or condition to check.
 *
 * Alias for FBP_DBC_ASSERT().
 */
#define FBP_DBC_REQUIRE(x) FBP_DBC_ASSERT((x), #x)

/** @} */

#endif /* FBP_FBP_DBC_H_ */
