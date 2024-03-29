/*
 * Copyright 2020-2021 Jetperch LLC
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
 * @brief Union type.
 */

#ifndef FBP_UNION_TYPE_H__
#define FBP_UNION_TYPE_H__

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_union Union type
 *
 * @brief A generic union type.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The allowed data types.
enum fbp_union_e {
    FBP_UNION_NULL = 0,  ///< NULL value.  Also used to clear existing value.
    FBP_UNION_STR = 1,   ///< UTF-8 string value, null terminated.
    FBP_UNION_JSON = 2,  ///< UTF-8 JSON string value, null terminated.
    FBP_UNION_BIN = 3,   ///< Raw binary value
    FBP_UNION_RSV0 = 4,  ///< Reserved, do not use
    FBP_UNION_RSV1 = 5,  ///< Reserved, do not use
    FBP_UNION_F32 = 6,   ///< 32-bit IEEE 754 floating point
    FBP_UNION_F64 = 7,   ///< 64-bit IEEE 754 floating point
    FBP_UNION_U8 = 8,    ///< Unsigned 8-bit integer value.
    FBP_UNION_U16 = 9,   ///< Unsigned 16-bit integer value.
    FBP_UNION_U32 = 10,  ///< Unsigned 32-bit integer value.
    FBP_UNION_U64 = 11,  ///< Unsigned 64-bit integer value.
    FBP_UNION_I8 = 12,   ///< Signed 8-bit integer value.
    FBP_UNION_I16 = 13,  ///< Signed 16-bit integer value.
    FBP_UNION_I32 = 14,  ///< Signed 32-bit integer value.
    FBP_UNION_I64 = 15,  ///< Signed 64-bit integer value.
};

/**
 * @brief The standardized Fitterbap flags.
 *
 * Applications may define custom flags in fbp_union_s.app.
 */
enum fbp_union_flag_e {
    /// No flags specified.
    FBP_UNION_FLAG_NONE = 0,

    /// The PubSub instance should retain this value.
    FBP_UNION_FLAG_RETAIN = (1 << 0),

    /// The value points to a const that will remain valid indefinitely.
    FBP_UNION_FLAG_CONST = (1 << 1),
};

/// The actual value holder for fbp_union_s.
union fbp_union_inner_u {
    const char * str;      ///< FBP_UNION_STR, FBP_UNION_JSON
    const uint8_t * bin;   ///< FBP_UNION_BIN
    float f32;             ///< FBP_UNION_F32
    double f64;            ///< FBP_UNION_F64
    uint8_t u8;            ///< FBP_UNION_U8
    uint16_t u16;          ///< FBP_UNION_U16
    uint32_t u32;          ///< FBP_UNION_U32
    uint64_t u64;          ///< FBP_UNION_U64
    int8_t i8;             ///< FBP_UNION_I8
    int16_t i16;           ///< FBP_UNION_I16
    int32_t i32;           ///< FBP_UNION_I32
    int64_t i64;           ///< FBP_UNION_I64
};

/// The value holder for all types.
struct fbp_union_s {
    uint8_t type;   ///< The fbp_union_e data format indicator.
    uint8_t flags;  ///< The fbp_union_flag_e flags.
    uint8_t op;     ///< The application-specific operation.
    uint8_t app;    ///< Application specific data.  If unused, write to 0.

    uint32_t size;  ///< payload size for pointer types, including null terminator for strings.

    /// The actual value.
    union fbp_union_inner_u value;
};

// Convenience value creation macros
#define fbp_union_null() ((struct fbp_union_s){.type=FBP_UNION_NULL, .op=0, .flags=0, .app=0, .value={.u64=0}, .size=0})
#define fbp_union_null_r() ((struct fbp_union_s){.type=FBP_UNION_NULL, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.u32=0}, .size=0})

#if FBP_CONFIG_USE_FLOAT32
#define fbp_union_f32(_value) ((struct fbp_union_s){.type=FBP_UNION_F32, .op=0, .flags=0, .app=0, .value={.f32=_value}, .size=0})
#define fbp_union_f32_r(_value) ((struct fbp_union_s){.type=FBP_UNION_F32, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.f32=_value}, .size=0})
#endif

#if FBP_CONFIG_USE_FLOAT64
#define fbp_union_f64(_value) ((struct fbp_union_s){.type=FBP_UNION_F64, .op=0, .flags=0, .app=0, .value={.f64=_value}, .size=0})
#define fbp_union_f64_r(_value) ((struct fbp_union_s){.type=FBP_UNION_F64, .op=0, .app=0, .flags=FBP_UNION_FLAG_RETAIN, .value={.f64=_value}, .size=0})
#endif

#define fbp_union_u8(_value) ((struct fbp_union_s){.type=FBP_UNION_U8, .op=0, .flags=0, .app=0, .value={.u8=_value}, .size=0})
#define fbp_union_u8_r(_value) ((struct fbp_union_s){.type=FBP_UNION_U8, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.u64=_value}, .size=0})
#define fbp_union_u16(_value) ((struct fbp_union_s){.type=FBP_UNION_U16, .op=0, .flags=0, .app=0, .value={.u16=_value}, .size=0})
#define fbp_union_u16_r(_value) ((struct fbp_union_s){.type=FBP_UNION_U16, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.u64=_value}, .size=0})
#define fbp_union_u32(_value) ((struct fbp_union_s){.type=FBP_UNION_U32, .op=0, .flags=0, .app=0, .value={.u32=_value}, .size=0})
#define fbp_union_u32_r(_value) ((struct fbp_union_s){.type=FBP_UNION_U32, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.u64=_value}, .size=0})
#define fbp_union_u64(_value) ((struct fbp_union_s){.type=FBP_UNION_U64, .op=0, .flags=0, .app=0, .value={.u64=_value}, .size=0})
#define fbp_union_u64_r(_value) ((struct fbp_union_s){.type=FBP_UNION_U64, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.u64=_value}, .size=0})

#define fbp_union_i8(_value) ((struct fbp_union_s){.type=FBP_UNION_I8, .op=0, .flags=0, .app=0, .value={.i8=_value}, .size=0})
#define fbp_union_i8_r(_value) ((struct fbp_union_s){.type=FBP_UNION_I8, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.i64=_value}, .size=0})
#define fbp_union_i16(_value) ((struct fbp_union_s){.type=FBP_UNION_I16, .op=0, .flags=0, .app=0, .value={.i16=_value}, .size=0})
#define fbp_union_i16_r(_value) ((struct fbp_union_s){.type=FBP_UNION_I16, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.i64=_value}, .size=0})
#define fbp_union_i32(_value) ((struct fbp_union_s){.type=FBP_UNION_I32, .op=0, .flags=0, .app=0, .value={.i32=_value}, .size=0})
#define fbp_union_i32_r(_value) ((struct fbp_union_s){.type=FBP_UNION_I32, .op=0, .flags= FBP_UNION_FLAG_RETAIN, .app=0, .value={.i64=_value}, .size=0})
#define fbp_union_i64(_value) ((struct fbp_union_s){.type=FBP_UNION_I64, .op=0, .flags=0, .app=0, .value={.i64=_value}, .size=0})
#define fbp_union_i64_r(_value) ((struct fbp_union_s){.type=FBP_UNION_I64, .op=0, .flags=FBP_UNION_FLAG_RETAIN, .app=0, .value={.i64=_value}, .size=0})

#define fbp_union_str(_value) ((struct fbp_union_s){.type=FBP_UNION_STR, .op=0, .flags=0, .app=0, .value={.str=_value}, .size=0})
#define fbp_union_cstr(_value) ((struct fbp_union_s){.type=FBP_UNION_STR, .op=0, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.str=_value}, .size=0})
#define fbp_union_cstr_r(_value) ((struct fbp_union_s){.type=FBP_UNION_STR, .op=0, .flags=FBP_UNION_FLAG_CONST | FBP_UNION_FLAG_RETAIN, .app=0, .value={.str=_value}, .size=0})

#define fbp_union_json(_value) ((struct fbp_union_s){.type=FBP_UNION_JSON, .op=0, .flags=0, .app=0, .value={.str=_value}, .size=0})
#define fbp_union_cjson(_value) ((struct fbp_union_s){.type=FBP_UNION_JSON, .op=0, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.str=_value}, .size=0})
#define fbp_union_cjson_r(_value) ((struct fbp_union_s){.type=FBP_UNION_JSON, .op=0, .flags=FBP_UNION_FLAG_CONST | FBP_UNION_FLAG_RETAIN, .app=0, .value={.str=_value}, .size=0})

#define fbp_union_bin(_value, _size) ((struct fbp_union_s){.type=FBP_UNION_BIN, .op=0, .flags=0, .app=0, .value={.bin=_value}, .size=_size})
#define fbp_union_cbin(_value, _size) ((struct fbp_union_s){.type=FBP_UNION_BIN, .op=0, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.bin=_value}, .size=_size})
#define fbp_union_cbin_r(_value, _size) ((struct fbp_union_s){.type=FBP_UNION_BIN, .op=0, .flags=FBP_UNION_FLAG_CONST | FBP_UNION_FLAG_RETAIN, .app=0, .value={.bin=_value}, .size=_size})

/**
 * @brief Check if two values are equal.
 *
 * @param v1 The first value.
 * @param v2 The second value.
 * @return True if equal, false if not equal.
 *
 * This check only compares the type and value.  The types and values must match
 * exactly.  However, if ignores the additional fields
 * [flags, op, app].  Use fbp_union_eq_strict() to also compare these fields.
 * Use fbp_union_equiv() to more loosely check the value.
 */
FBP_API bool fbp_union_eq(const struct fbp_union_s * v1, const struct fbp_union_s * v2);

/**
 * @brief Check if two values are equal.
 *
 * @param v1 The first value.
 * @param v2 The second value.
 * @return True if equal, false if not equal.
 *
 * This check strictly compares every field.
 */
FBP_API bool fbp_union_eq_exact(const struct fbp_union_s * v1, const struct fbp_union_s * v2);

/**
 * @brief Check if two values are equivalent.
 *
 * @param v1 The first value.
 * @param v2 The second value.
 * @return True if equal, false if not equal.
 *
 * This check performs type up-conversions to attempt to match the
 * two fields.
 */
FBP_API bool fbp_union_equiv(const struct fbp_union_s * v1, const struct fbp_union_s * v2);

/**
 * @brief Widen to the largest-size, compatible numeric type.
 *
 * @param x The value to widen in place.
 * @see fbp_union_as_type()
 *
 * This widening conversion preserves float, signed, and unsigned characteristics.
 * This check performs type up-conversions to attempt to match the
 * two fields.
 */
FBP_API void fbp_union_widen(struct fbp_union_s * x);

/**
 * @brief Convert value to a specific type.
 *
 * @param x The value to convert in place.
 * @param type The target type.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_union_as_type(struct fbp_union_s * x, uint8_t type);

/**
 * @brief Convert a value to a boolean.
 *
 * @param value The value to convert.  For numeric types, and non-zero value is
 *      presumed true.  String and JSON compare against a case insensitive
 *      string list using fbp_cstr_to_bool..  True is ["true", "on", "enable", "enabled", "yes"] and
 *      False is ["false", "off", "disable", "disabled", "no"].  All other values return
 *      an error.
 * @param rv The resulting boolean value.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_union_to_bool(const struct fbp_union_s * value, bool * rv);

/**
 * @brief Check if the union contains a pointer type.
 *
 * @param value The union value.
 * @return True is the union contains a pointer type, false otherwise.
 */
static inline bool fbp_union_is_type_ptr(const struct fbp_union_s * value) {
    switch (value->type) {
        case FBP_UNION_STR:  // intentional fall-through
        case FBP_UNION_JSON:  // intentional fall-through
        case FBP_UNION_BIN:  // intentional fall-through
            return true;
        default:
            return false;
    }
}

/**
 * @brief Convert the type to a user-meaningful string.
 *
 * @param type The fbp_union_e type.
 * @return The user-meaningful string representation for the type.
 */
FBP_API const char * fbp_union_type_to_str(uint8_t type);

/**
 * @brief Convert the value to a user-meaningful string.
 *
 * @param value The value.
 * @param str[out] The string to hold the value.
 * @param str_len The maximum length of str, in bytes.
 * @param opts The formatting options. 0=value only, 1=verbose with type and flags.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_union_value_to_str(const struct fbp_union_s * value, char * str, uint32_t str_len, uint32_t opts);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_UNION_TYPE_H__ */
