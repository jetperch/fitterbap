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
 * @brief Platform for the C standard library
 */

#ifndef FBP_PLATFORM_STDLIB_H_
#define FBP_PLATFORM_STDLIB_H_

#include "fitterbap/platform.h"
#include "fitterbap/assert.h"
#include <stdlib.h>
#include <string.h> // memcpy, memset

FBP_CPP_GUARD_START

#if 0
// straightforward bit shift implementation
static inline uint32_t fbp_clz(uint32_t x) {
    uint32_t c = 0;
    uint32_t m = 0x80000000U;
    while ((c < 32) && (0 == (x & m))) {
        ++c;
        m = m >> 1;
    }
    return c;
}
#else

#define check_bits(bits) \
    y = x >> bits; \
    if (y) { \
        leading_zeros -= bits; \
        x = y; \
    }

// Divide & conquer implementation
static inline uint32_t fbp_clz(uint32_t x) {
    uint32_t leading_zeros = 32;
    uint32_t y;
    check_bits(16);
    check_bits(8);
    check_bits(4);
    check_bits(2);
    check_bits(1);
    leading_zeros -= x;
    return leading_zeros;
}
#endif

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
// 1 << (32 - clz) is even faster with native CLZ support.
static inline uint32_t fbp_upper_power_of_two(uint32_t x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

static inline void fbp_memset(void * ptr, int value, fbp_size_t num) {
    memset(ptr, value, num);
}

static inline void fbp_memcpy(void * destination, void const * source, fbp_size_t num) {
    memcpy(destination, (void *) source, num);
}

FBP_CPP_GUARD_END

#endif /* FBP_PLATFORM_STDLIB_H_ */
