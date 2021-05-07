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
 * @brief Platform for ARM.
 */

#ifndef FBP_PLATFORM_ARM_H_
#define FBP_PLATFORM_ARM_H_

#include "fitterbap/platform.h"
#include "fitterbap/assert.h"
#include <string.h>  // use memset and memcpy from the standard library

FBP_CPP_GUARD_START

static inline uint32_t fbp_clz(uint32_t x) {
    uint32_t leading_zeros;
    __asm volatile ( "clz %0, %1" : "=r" ( leading_zeros ) : "r" ( x ) );
    return leading_zeros;
}

static inline uint32_t fbp_upper_power_of_two(uint32_t x) {
    uint32_t pow = 32 - fbp_clz(x - 1);
    return (1 << pow);
}

static inline void fbp_memset(void * ptr, int value, fbp_size_t num) {
    memset(ptr, value, num);
}

static inline void fbp_memcpy(void * destination, void const * source, fbp_size_t num) {
    memcpy(destination, (void *) source, num);
}

FBP_CPP_GUARD_END

#endif /* FBP_PLATFORM_ARM_H_ */
