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

#include "fitterbap/lfsr.h"
#include "fitterbap/dbc.h"
#include "fitterbap.h"

void fbp_lfsr_initialize(struct fbp_lfsr_s * self) {
    FBP_DBC_NOT_NULL(self);
    self->value = LFSR16_INITIAL_VALUE;
    self->error_count = 0;
    self->resync_bit_count = 16;
}

void fbp_lfsr_seed_u16(struct fbp_lfsr_s * self, uint16_t seed) {
    FBP_DBC_NOT_NULL(self);
    if (seed == 0) {
        seed = 1;
    }
    self->value = seed;
}

static inline void value_guard(struct fbp_lfsr_s * self) {
    FBP_DBC_NOT_NULL(self);
    if (self->value == 0) {
        FBP_LOG_WARNING("Invalid lfsr value");
        self->value = 1;
    }
}

static inline int fbp_lfsr_next_u1_inner(struct fbp_lfsr_s * self) {
    int bit = 0;
    uint16_t lfsr = self->value;
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    self->value = (lfsr >> 1) | (bit << 15);
    return bit;
}

int fbp_lfsr_next_u1(struct fbp_lfsr_s * self) {
    value_guard(self);
    return fbp_lfsr_next_u1_inner(self);
}

uint8_t fbp_lfsr_next_u8(struct fbp_lfsr_s * self) {
    int i = 0;
    value_guard(self);
    for (i = 0; i < 8; ++i) {
        fbp_lfsr_next_u1(self);
    }
    return (uint8_t) ((self->value & 0xff00) >> 8);
}

uint16_t fbp_lfsr_next_u16(struct fbp_lfsr_s * self) {
    int i = 0;
    value_guard(self);
    for (i = 0; i < 16; ++i) {
        fbp_lfsr_next_u1(self);
    }

    return self->value;
}

uint32_t fbp_lfsr_next_u32(struct fbp_lfsr_s * self) {
    uint32_t value = 0;
    value = ((uint32_t) fbp_lfsr_next_u16(self)) << 16;
    value |= (uint32_t) fbp_lfsr_next_u16(self);
    return value;
}

int fbp_lfsr_follow_u8(struct fbp_lfsr_s * self, uint8_t data) {
    uint8_t expected;
    FBP_DBC_NOT_NULL(self);
    if (self->resync_bit_count) {
        self->value = (self->value >> 8) | (((uint16_t) data) << 8);
        self->resync_bit_count -= 8;
        if (self->resync_bit_count < 0) {
            self->resync_bit_count = 0;
        }
        return 0;
    }
    expected = fbp_lfsr_next_u8(self);
    if (data == expected) {
        return 0;
    } else {
        self->value = ((uint16_t) data) << 8;
        self->resync_bit_count = 8;
        ++self->error_count;
        return -1;
    }
}
