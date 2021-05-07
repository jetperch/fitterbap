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
 * @brief Support encoding and decoding from byte buffers.
 */

#ifndef FBP_MEMORY_BBUF_H_
#define FBP_MEMORY_BBUF_H_

#include "fitterbap/platform.h"

/**
 * @ingroup fbp_memory
 * @defgroup fbp_memory_bbuf Byte buffer
 *
 * @brief Encoding and decoding from byte buffers.
 *
 * This module contains several different mechanisms for encoding and
 * decoding from byte buffers.  The macros are completely memory unsafe
 * but very fast.  The unsafe functions should inline and track the current
 * cursor location, but will not respect the buffer end.  The safe functions
 * fully verify every operation for memory safety and are recommend except
 * when performance is critical.
 *
 * This entire module is very pedantic.  However, many memory violations and
 * buffer overflows result from bad serialization and deserialization,
 * something this module hopes to improve.  It also alleviates many common
 * typing errors that can be difficult to identify.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @defgroup fbp_bbuf_macro Unsafe buffer macros
 *
 * @brief Support efficient but memory-unsafe encoding and decoding from
 *      byte buffers using macros.
 *
 * These macros are NOT memory safe to enable high performance for packing
 * and unpacking structures.  The caller is responsible for ensuring
 * that sufficient buffer space exists BEFORE calling these functions.
 *
 * @{
 */

/**
 * @brief Encode a uint8 to the buffer.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U8(buffer, value) (buffer)[0] = (value)

/**
 * @brief Encode a uint16 to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U16_BE(buffer, value) \
        (buffer)[0] = ((value) >> 8) & 0xff; \
        (buffer)[1] = ((value)     ) & 0xff

/**
 * @brief Encode a uint16 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U16_LE(buffer, value) \
        (buffer)[0] = ((value)     ) & 0xff; \
        (buffer)[1] = ((value) >> 8) & 0xff

/**
 * @brief Encode a uint32 to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U32_BE(buffer, value) \
        FBP_BBUF_ENCODE_U16_BE((buffer), ( (value) >> 16)); \
        FBP_BBUF_ENCODE_U16_BE((buffer) + 2, (value) )

/**
 * @brief Encode a uint32 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U32_LE(buffer, value) \
        FBP_BBUF_ENCODE_U16_LE((buffer), (value) ); \
        FBP_BBUF_ENCODE_U16_LE((buffer) + 2, (value) >> 16)

/**
 * @brief Encode a uint64to the buffer in big-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U64_BE(buffer, value) \
        FBP_BBUF_ENCODE_U32_BE((buffer), ( (value) >> 32)); \
        FBP_BBUF_ENCODE_U32_BE((buffer) + 4, (value) )

/**
 * @brief Encode a uint64 to the buffer in little-endian order.
 *
 * @param[in] buffer The pointer to the buffer pointer.
 * @param[in] value The value to add to the buffer.
 */
#define FBP_BBUF_ENCODE_U64_LE(buffer, value) \
        FBP_BBUF_ENCODE_U32_LE((buffer), (value) ); \
        FBP_BBUF_ENCODE_U32_LE((buffer) + 4, (value) >> 32)

/**
 * @brief Decode a uint8 from the buffer.
 *
 * @param[in] buffer The pointer to the buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U8(buffer) (buffer)[0];

/**
 * @brief Decode a uint16 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U16_BE(buffer) ( \
        (((uint16_t) ((buffer)[0])) <<  8) | \
        (((uint16_t) ((buffer)[1]))      ) )

/**
 * @brief Decode a uint16 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U16_LE(buffer) ( \
        (((uint16_t) ((buffer)[0]))     ) | \
        (((uint16_t) ((buffer)[1])) << 8) )

/**
 * @brief Decode a uint32 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U32_BE(buffer) ( \
        (((uint32_t) FBP_BBUF_DECODE_U16_BE(buffer)) << 16) | \
        ((uint32_t) FBP_BBUF_DECODE_U16_BE(buffer + 2)) )

/**
 * @brief Decode a uint32 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U32_LE(buffer) ( \
        ((uint32_t) FBP_BBUF_DECODE_U16_LE(buffer)) | \
        (((uint32_t) FBP_BBUF_DECODE_U16_LE(buffer + 2)) << 16) )

/**
 * @brief Decode a uint64 from the buffer in big-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U64_BE(buffer) ( \
        (((uint64_t) FBP_BBUF_DECODE_U32_BE(buffer)) << 32) | \
        ((uint64_t) FBP_BBUF_DECODE_U32_BE(buffer + 4)) )

/**
 * @brief Decode a uint64 from the buffer in little-endian order.
 *
 * @param[in] buffer The buffer.
 * @return The value decoded from the buffer.
 */
#define FBP_BBUF_DECODE_U64_LE(buffer) ( \
        ((uint64_t) FBP_BBUF_DECODE_U32_LE(buffer)) | \
        (((uint64_t) FBP_BBUF_DECODE_U32_LE(buffer + 4)) << 32) )


/** @} */

/**
 * @defgroup fbp_bbuf_unsafe Unsafe buffer functions
 *
 * @brief Support efficient but memory-unsafe encoding and decoding from
 *      byte buffers.
 *
 * These functions are NOT memory safe to enable high performance for packing
 * and unpacking structures.  The caller is responsible for ensuring
 * that sufficient buffer space exists BEFORE calling these functions.
 *
 * These functions uses the "inline" keyword which requires C99 (not ANSI-C).
 *
 * @{
 */

/**
 * @brief Encode a uint8 to the buffer.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u8(uint8_t ** buffer, uint8_t value) {
    FBP_BBUF_ENCODE_U8(*buffer, value);
    *buffer += 1;
}

/**
 * @brief Encode a uint16 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u16_be(uint8_t ** buffer, uint16_t value) {
    FBP_BBUF_ENCODE_U16_BE(*buffer, value);
    *buffer += 2;
}

/**
 * @brief Encode a uint16 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u16_le(uint8_t ** buffer, uint16_t value) {
    FBP_BBUF_ENCODE_U16_LE(*buffer, value);
    *buffer += 2;
}

/**
 * @brief Encode a uint32 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u32_be(uint8_t ** buffer, uint32_t value) {
    FBP_BBUF_ENCODE_U32_BE(*buffer, value);
    *buffer += 4;
}

/**
 * @brief Encode a uint32 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u32_le(uint8_t ** buffer, uint32_t value) {
    FBP_BBUF_ENCODE_U32_LE(*buffer, value);
    *buffer += 4;
}

/**
 * @brief Encode a uint64 to the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u64_be(uint8_t ** buffer, uint64_t value) {
    FBP_BBUF_ENCODE_U64_BE(*buffer, value);
    *buffer += 8;
}

/**
 * @brief Encode a uint64 to the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer.  The target data is
 *      populated with value and the pointer is advanced.
 * @param[in] value The value to add to the buffer.
 */
static inline void fbp_bbuf_unsafe_encode_u64_le(uint8_t ** buffer, uint64_t value) {
    FBP_BBUF_ENCODE_U64_LE(*buffer, value);
    *buffer += 8;
}

/**
 * @brief Decode a uint8 from the buffer.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint8_t fbp_bbuf_unsafe_decode_u8(uint8_t const ** buffer) {
    uint8_t v = FBP_BBUF_DECODE_U8(*buffer);
    *buffer += 1;
    return v;
}

/**
 * @brief Decode a uint16 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint16_t fbp_bbuf_unsafe_decode_u16_be(uint8_t const ** buffer) {
    uint16_t v = FBP_BBUF_DECODE_U16_BE(*buffer);
    *buffer += 2;
    return v;
}

/**
 * @brief Decode a uint16 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint16_t fbp_bbuf_unsafe_decode_u16_le(uint8_t const ** buffer) {
    uint16_t v = FBP_BBUF_DECODE_U16_LE(*buffer);
    *buffer += 2;
    return v;
}

/**
 * @brief Decode a uint32 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint32_t fbp_bbuf_unsafe_decode_u32_be(uint8_t const ** buffer) {
    uint32_t v = FBP_BBUF_DECODE_U32_BE(*buffer);
    *buffer += 4;
    return v;
}

/**
 * @brief Decode a uint32 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint32_t fbp_bbuf_unsafe_decode_u32_le(uint8_t const ** buffer) {
    uint32_t v = FBP_BBUF_DECODE_U32_LE(*buffer);
    *buffer += 4;
    return v;
}

/**
 * @brief Decode a uint64 from the buffer in big-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint64_t fbp_bbuf_unsafe_decode_u64_be(uint8_t const ** buffer) {
    uint64_t v = FBP_BBUF_DECODE_U64_BE(*buffer);
    *buffer += 8;
    return v;
}

/**
 * @brief Decode a uint64 from the buffer in little-endian order.
 *
 * @param[inout] buffer The pointer to the buffer pointer containing the data
 *      to decode.  The pointer is advance.
 * @return The value decoded from the buffer.
 */
static inline uint64_t fbp_bbuf_unsafe_decode_u64_le(uint8_t const ** buffer) {
    uint64_t v = FBP_BBUF_DECODE_U64_LE(*buffer);
    *buffer += 8;
    return v;
}

/** @} */

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_MEMORY_BBUF_H_ */
