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
 * @brief Linear feedback shift register pseudo-random number generator.
 */

#ifndef FBP_LFSR_H_
#define FBP_LFSR_H_

#include <stdint.h>
#include "fitterbap/cmacro_inc.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_lfsr LFSR
 *
 * @brief Linear feedback shift register pseudo-random number generator.
 *
 * @see https://en.wikipedia.org/wiki/Linear_feedback_shift_register.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The initial value for LFSR 16.
 */
#define LFSR16_INITIAL_VALUE 0xACE1u

/**
 * @brief The number of values before the LFSR repreats.
 */
#define LFSR16_LENGTH 65535

/**
 * @brief The LFSR state.
 */
struct fbp_lfsr_s {
    /**
     * @brief The current state value.
     */
    uint16_t value;

    /**
     * @brief The total number of errors while following a stream.
     */
    int error_count;

    /**
     * @brief The number of bits left before resynchronization completes.
     */
    int resync_bit_count;
};

/**
 * @brief Initialize the LFSR state.
 *
 * @param self [inout] The state to initialize.
 * @return 0 on success or error code.
 *
 * The 16-bit polynomial is x16 + x14 + x13 + x11 + 1.
 */
FBP_API void fbp_lfsr_initialize(struct fbp_lfsr_s * self);

/**
 * @brief Seed the LFSR.
 *
 * @param self [inout] The state to initialize.
 * @param seed [in] The new value seed for the LFSR.
 * @return 0 on success or error code.
 */
FBP_API void fbp_lfsr_seed_u16(struct fbp_lfsr_s * self, uint16_t seed);

/**
 * @brief Get the next bit in an LFSR pattern.
 *
 * @param self [inout] The LFSR state.
 * @return The next bit (0 or 1) in the LFSR sequence.
 *
 */
FBP_API int fbp_lfsr_next_u1(struct fbp_lfsr_s * self);

/**
 * @brief Get the next 8-bit value in an LFSR pattern.
 *
 * @param self [inout] The LFSR state.
 * @return The next 8-bit value in the LFSR sequence.
 *
 */
FBP_API uint8_t fbp_lfsr_next_u8(struct fbp_lfsr_s * self);

/**
 * @brief Get the next 16-bit value in an LFSR pattern.
 *
 * @param self [inout] The LFSR state.
 * @return The next 16-bit value in the LFSR sequence.
 *
 */
FBP_API uint16_t fbp_lfsr_next_u16(struct fbp_lfsr_s * self);

/**
 * @brief Get the next 32-bit value in an LFSR pattern.
 *
 * @param self [inout] The LFSR state.
 * @return The next 32-bit value in the LFSR sequence.
 *
 */
FBP_API uint32_t fbp_lfsr_next_u32(struct fbp_lfsr_s * self);

/**
 * @brief Follow the next 8-bit value in an LFSR pattern.
 *
 * @param self [inout] The LFSR state.
 * @param data [in] The next 8-bit data value for the stream.
 * @return 0 on success, -1 on unexpected data value, 1 on error.
 */
FBP_API int fbp_lfsr_follow_u8(struct fbp_lfsr_s * self, uint8_t data);

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_LFSR_H_ */
