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

#ifndef FBP_HOST_PLATFORM_ARM_H_
#define FBP_HOST_PLATFORM_ARM_H_

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/config.h"
#include "fitterbap/config_defaults.h"
#include "fitterbap/platform_dependencies.h"


FBP_CPP_GUARD_START

FBP_INLINE_FN uint32_t fbp_clz(uint32_t x) {
    uint32_t leading_zeros;
    __asm volatile ( "clz %0, %1" : "=r" ( leading_zeros ) : "r" ( x ) );
    return leading_zeros;
}

FBP_INLINE_FN uint32_t fbp_upper_power_of_two(uint32_t x) {
    if (x == 0) {
        return 0;
    }
    return 32 - fbp_clz(x - 1);
}

FBP_CPP_GUARD_END

#endif /* FBP_HOST_PLATFORM_ARM_H_ */
