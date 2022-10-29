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
 * @brief Holding place for commonly used C macros.
 */

#ifndef FBP_CDEF_H_
#define FBP_CDEF_H_

#include "fitterbap/common_header.h"
#include <stddef.h>  // offsetof

/**
 * @ingroup fbp_core
 * @defgroup fbp_cdef Common C utilities
 *
 * @brief Common C macros that are not built into standard C.
 *
 * @{
 */

/**
 * @brief Perform a compile-time check
 *
 * @param COND The condition which should normally be true.
 * @param MSG The error message which must be a valid C identifier  This
 *      message will be cryptically displayed by the compiler on error.
 */
#define FBP_STATIC_ASSERT(COND, MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

/**
 * @brief Compute the number of elements in an array.
 *
 * @param x The array (NOT a pointer).
 * @return The number of elements in the array.
 */
#define FBP_ARRAY_SIZE(x) ((fbp_size_t) ( sizeof(x) / sizeof((x)[0]) ))

/**
 * @brief Initialize a structure to zero.
 *
 * @param s The structure to clear.
 */
#define FBP_STRUCT_INIT(s) (fbp_memset(&s, 0, fbp_sizeof(s)))

/**
 * @brief Initialize a structure to zero given a pointer.
 *
 * @param s_ptr The pointer to the structure to clear.
 */
#define FBP_STRUCT_PTR_INIT(s_ptr) (fbp_memset(s_ptr, 0, fbp_sizeof(*s_ptr)))

/**
 * @brief Evaluate and return on error.
 *
 * @param x The value to evaluate which results in an int32_t.
 *      Any non-zero value is presumed to be an error.
 *
 * On error, cause the calling function to return with the error code.
 * Modules using this macro typically define a shortened version, such as:
 *
 *     #define RWE FBP_RETURN_ON_ERROR
 */
#define FBP_RETURN_ON_ERROR(x) do { \
    int32_t rc__ = (x); \
    if (rc__) { \
        return rc__; \
    } \
} while (0)

/**
 * @brief Evaluate and return when the expression is non-zero.
 *
 * @param x The value to evaluate which results in an int32_t.
 *      Any non-zero value is presumed to be an error.
 * @param msg The message to log on error.
 * @see RETURN_ON_ERROR
 *
 * On error, cause the calling function to return with the error code.
 */
#define FBP_RETURN_ON_ERROR_MSG(x, msg) do { \
    int32_t rc__ = (x); \
    if (rc__) {\
        FBP_LOGE("[%d:%s] %s", rc__, fbp_error_code_name(rc__), msg); \
        return rc__; \
    } \
} while (0)

/**
 * @brief Evaluate and goto function "exit" label on error.
 * @param x The value to evaluate which results in an int32_t.
 *      Any non-zero value is presumed to be an error.
 */
#define FBP_EXIT_ON_ERROR(x) do { \
    int32_t rc__ = (x); \
    if (rc__) { \
        goto exit; \
    } \
} while (0)

/**
 * @brief Evaluate and goto function "exit" label on error.
 *
 * @param var The variable to assign with the error code.
 * @param x The value to evaluate which results in an int32_t.
 *      Any non-zero value is presumed to be an error and is
 *      assigned to var.
 */
#define FBP_EXIT_WITH_ERROR(var, x) do { \
    int32_t rc__ = (x); \
    if (rc__) { \
        var = rc__; \
        goto exit; \
    } \
} while (0)

/**
 * @brief Restrict the value to within the range.
 * 
 * @param x_val The input value.
 * @param x_min The minimum allowed value for x.  Smaller x values return x_min.
 * @param x_max The maximum allowed value for x.  Larger x values return x_max.
 * @return x_val in the range [x_min, x_max].
 *
 * WARNING: The values for x_val, x_min and x_max must not have any side
 * effects as the parameters occur multiple times in the macro!
 */
#define FBP_RESTRICT_TO_RANGE(x_val, x_min, x_max) \
    ( ( (x_val) < (x_min) ) ? (x_min) : \
      ( ( (x_val) < (x_max) ) ? (x_val) : (x_max) ) )

/**
 * @brief Compute the signum of x.
 *
 * @param x The value for the sigum
 * @return The signum:
 *      * -1 when x < 0
 *      * 0 when x == 0
 *      * 1 when x > 0
 *
 * WARNING: The value for x must not have any side effects
 * as the parameter occurs multiple times in the macro!
 */
#define FBP_SIGNUM(x) ((0 < (x) ) - ((x) < 0))

/**
 * @brief Round a integer value up (away from zero) to the nearest multiple.
 *
 * @param x The integer value to round away from zero.  For positive integers,
 *      this rounds up towards positive infinity.  For negative integers,
 *      this rounds down towards negative infinity.
 * @param m The multiple for rounding.
 * @return The value of x rounded up to m.  The output is undefined
 *      for negative values of x.
 *
 * Some examples:
 *
 *     ROUND_UP_TO_MULTIPLE(0, 128) => 128
 *     ROUND_UP_TO_MULTIPLE(1, 128) => 128
 *     ROUND_UP_TO_MULTIPLE(128, 128) => 128
 *     ROUND_UP_TO_MULTIPLE(129, 128) => 256
 *     ROUND_UP_TO_MULTIPLE(-1, 128) => -128
 *
 * WARNING: The values for x and m MUST NOT have any side effects
 * as the parameters occur multiple times in the macro!
 */
#define FBP_ROUND_UP_TO_MULTIPLE(x, m) (( ((x) + ( FBP_SIGNUM(x) * (m - 1) )) / m) * m)

/**
 * @brief Round an unsigned integer value up to the nearest multiple.
 *
 * @param x The integer value to round up towards positive infinity.
 * @param m The multiple for rounding.
 * @return The value of x rounded up to m.  The output is undefined
 *      for negative values of x.
 *
 * @see FBP_ROUND_UP_TO_MULTIPLE() for signed integers.
 */
#define FBP_ROUND_UP_TO_MULTIPLE_UNSIGNED(x, m) (( ((x) + (m - 1) ) / m) * m)

/**
 * @brief Find the container instance given a member pointer.
 *
 * @param ptr The pointer to an instance member.
 * @param type The type of the container that has member.
 * @param member The name of the member targeted by ptr.
 * @return The pointer to the container.
 */
#define FBP_CONTAINER_OF(ptr, type, member) \
    ( (type *) (((char *) (ptr)) - offsetof(type, member)) )


#ifdef __cplusplus

/**
 * @brief Perform compile-time sizeof print (causes compilation to fail!)
 *
 * @param t The structure, class or type for the print.
 */
#define STATIC_PRINT_SIZEOF(t) \
    template<int s> struct sizeof_ ## t ## _is ; \
    struct sizeof_ ## t ## _is  <sizeof(t)> t ## _sz

/**
 * @brief Perform compile-time print of an integer (causes compilation to fail!)
 *
 * @param t The integer for the print.
 */
#define STATIC_PRINT_INT(i, name) \
    template<int s> struct int_ ## name ## _is; \
    struct int_ ## name ## _is <i> name ## _sz;

#endif

/** @} */

#endif /* FBP_CDEF_H_ */

