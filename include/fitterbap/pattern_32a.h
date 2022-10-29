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
 * @brief Generate and track a simple 32-bit test pattern for data path testing.
 */

#ifndef FBP_PATTERN_32A_H
#define FBP_PATTERN_32A_H

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_pattern_32a Test patterns
 *
 * @brief Generate and receive a test pattern consisting of 32-bit values.
 *
 * The pattern consists of two interleaved patterns that alternate with
 * each 32-bit value.
 *
 * The first pattern is a single bit which is shifted from
 * the least significant bit to the most significant bit and then to zero.
 * Once the value is zero, the least significant bit is set again.  The pattern
 * period is 33 values (value 0 + 32 values for each bit set).  This pattern
 * allows rapid detection of shorts and opens.
 *
 * The second pattern is a 16-bit counter which contains the counter and
 * bit inverse in the upper 16-bits.  The period is 2^16 = 65536.  This
 * pattern creates a much longer period suitable for reliability detecting
 * lost/duplicated words and blocks in the datapath.
 *
 * Since 33 and 65536 share no common denominators, the total period is
 * 33 * 65536 * 2 = 4325376 words.  The additional factor of 2 is because the
 * patterns are interleaved for 2 words per total pattern value.
 *
 * This pattern generator can reliably detect repeated words and any skips up
 * to half the the total period.  Skips longer than half the total period
 * are presumed to be duplicates.  The receiver only takes two consecutive
 * correct words to resynchronize to the transmitter.
 *
 * @{
 */

FBP_CPP_GUARD_START

#define FBP_PATTERN_32A_PERIOD (4325376)

/**
 * @brief The test pattern generator state.
 */
struct fbp_pattern_32a_tx_s {
    uint32_t shift32;   ///< The current shift value.
    uint16_t counter;   ///< The current counter value.
    uint8_t toggle;     ///< The next value type: 0 = shift, 1 = counter
};

/**
 * @brief The test pattern receiver state.
 */
struct fbp_pattern_32a_rx_s {
    struct fbp_pattern_32a_tx_s tx;  ///< Internal receiver state
    uint64_t receive_count;   ///< Number of words received
    uint64_t missing_count;   ///< Estimated number of missing words
    uint64_t duplicate_count; ///< Estimated number of duplicate words
    uint64_t error_count;     ///< Words that could not be synchronized
    uint32_t resync_count;    ///< Number of resyncs
    uint32_t syncword1;       ///< The first synchronization word
    uint8_t state;            ///< Internal receiver state
};

/**
 * @brief Initialize the test pattern generator instance.
 *
 * @param self The instance to initialize.
 *
 * This function may be called at any time to reinitialize the state.
 */
FBP_API void fbp_pattern_32a_tx_initialize(
        struct fbp_pattern_32a_tx_s * self);

/**
 * @brief Generate the next value in the test pattern.
 *
 * @param self The instance to initialize.
 * @return The next 32-bit value in the pattern.
 */
FBP_API uint32_t fbp_pattern_32a_tx_next(
        struct fbp_pattern_32a_tx_s * self);

/**
 * @brief Fill a buffer with the next values in the pattern.
 *
 * @param self The instance to initialize.
 * @param buffer The buffer to fill.  Must be 32-bit word aligned.
 * @param size The size of buffer in total_bytes.  Must be a multiple of 4.
 */

FBP_API void fbp_pattern_32a_tx_buffer(
        struct fbp_pattern_32a_tx_s * self,
        uint32_t * buffer,
        uint32_t size);

/**
 * @brief Initialize the test pattern receiver instance.
 *
 * @param self The receiver instance to initialize.
 */
FBP_API void fbp_pattern_32a_rx_initialize(
        struct fbp_pattern_32a_rx_s * self);

/**
 * @brief Process the next received value.
 *
 * @param self The receiver instance.
 * @param value The next received value.
 */
FBP_API void fbp_pattern_32a_rx_next(
        struct fbp_pattern_32a_rx_s * self,
        uint32_t value);

/**
 * @brief Process the next received value(s) from a buffer.
 *
 * @param self The receiver instance.
 * @param buffer The buffer to process.  Must be 32-bit word aligned.
 * @param size The size of buffer in total_bytes.  Must be a multiple of 4.
 */
FBP_API void fbp_pattern_32a_rx_buffer(
        struct fbp_pattern_32a_rx_s * self,
        uint32_t const * buffer,
        uint32_t size);

FBP_CPP_GUARD_END

/** @} */

#endif // FBP_PATTERN_32A_H
