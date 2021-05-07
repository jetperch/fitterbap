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
 * @brief Argument checking.
 */

#ifndef FBP_ARGCHK_H_
#define FBP_ARGCHK_H_

#include "log.h"
#include "ec.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_argchk Argument checking and validation
 *
 * @brief Macros and functions to support argument checking.
 *
 * This module provides input argument checking for functions that return
 * an error code when the argument is invalid.  For functions that assert
 * and do not return on errors, see dbc for design-by-contract.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The default return code.
 */
#define FBP_ARGCHK_FAIL_RETURN_CODE_DEFAULT FBP_ERROR_PARAMETER_INVALID

/**
 * @def FBP_ARGCHK_FAIL_RETURN_CODE
 *
 * @brief The return code for argument check failure.
 *
 * Defaults to FBP_ARGCHK_FAIL_RETURN_CODE_DEFAULT.
 */
#ifndef FBP_ARGCHK_FAIL_RETURN_CODE
#define FBP_ARGCHK_FAIL_RETURN_CODE FBP_ARGCHK_FAIL_RETURN_CODE_DEFAULT
#endif


/**
 * @brief Check that an argument is true.
 *
 * @param condition The condition for the check which is expected to be 
 *      a truthy C value, as would be provided to assert.  When false, the 
 *      check fails and the macro causes the enclosing function to return with
 *      an error.
 * @param message The message to display when condition is false.
 */
#define FBP_ARGCHK_ASSERT(condition, message) do { \
    if (!(condition)) { \
        FBP_LOGI("chk_assert: %s", (message)); \
        return FBP_ARGCHK_FAIL_RETURN_CODE; \
    } \
} while (0)

/**
 * @brief Check for a "true" value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_ARGCHK_TRUE(x) FBP_ARGCHK_ASSERT((x), #x " is false")

/**
 * @brief Check for a "false" value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_ARGCHK_FALSE(x) FBP_ARGCHK_ASSERT(!(x), #x " is true")
    
/**
 * @brief Check for a non-null value.
 *
 * @param x The expression which should not be null.
 */
#define FBP_ARGCHK_NOT_NULL(x) FBP_ARGCHK_ASSERT((x) != 0, #x " is null")

/**
 * @brief Assert that a function argument is greater than zero.
 *
 * @param x The function argument to check.
 */
#define FBP_ARGCHK_GT_ZERO(x) FBP_ARGCHK_ASSERT((x) > 0, #x " <= 0")

/**
 * @brief Assert that a function argument is greater than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_ARGCHK_GTE_ZERO(x) FBP_ARGCHK_ASSERT((x) >= 0, #x " < 0")

/**
 * @brief Assert that a function argument is not equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_ARGCHK_NE_ZERO(x) FBP_ARGCHK_ASSERT((x) != 0, #x " != 0")

/**
 * @brief Assert that a function argument is less than zero.
 *
 * @param x The function argument to check.
 */
#define FBP_ARGCHK_LT_ZERO(x) FBP_ARGCHK_ASSERT((x) < 0, #x " >= 0")

/**
 * @brief Assert that a function argument is less than or equal to zero.
 *
 * @param x The function argument to check.
 */
#define FBP_ARGCHK_LTE_ZERO(x) FBP_ARGCHK_ASSERT((x) <= 0, #x " > 0")

/**
 * @brief Assert that a function argument is less than or equal to zero.
 *
 * @param x The function argument to check.
 * @param x_min The minimum value, inclusive.
 * @param x_max The maximum value, inclusive.
 */
#define FBP_ARGCHK_RANGE_INT(x, x_min, x_max)  do { \
    int x__ = (x); \
    int x_min__ = (x_min); \
    int x_max__ = (x_max); \
    if (x__ < x_min__) { \
        FBP_LOGI("chk_assert: %s [%d] < %d", #x, x__, x_min__); \
        return FBP_ARGCHK_FAIL_RETURN_CODE; \
    } \
    if (x__ > x_max__) { \
        FBP_LOGI("chk_assert: %s [%d] > %d", #x, x__, x_max__); \
        return FBP_ARGCHK_FAIL_RETURN_CODE; \
    } \
} while (0)

/**
 * @brief Assert on a function argument condition.
 *
 * @param x The function argument or condition to check.
 */
#define FBP_ARGCHK_REQUIRE(x) FBP_ARGCHK_ASSERT((x), #x)

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_ARGCHK_H_ */
