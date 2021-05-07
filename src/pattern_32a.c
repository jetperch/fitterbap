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

#include "fitterbap/pattern_32a.h"
#include "fitterbap/assert.h"
#include "fitterbap/dbc.h"
#include <stdbool.h>

#define TOGGLE_SHIFT ((uint8_t) 0)
#define TOGGLE_COUNT ((uint8_t) ~0)
#define SHIFT_PERIOD (33)


void fbp_pattern_32a_tx_initialize(struct fbp_pattern_32a_tx_s * self) {
    FBP_DBC_NOT_NULL(self);
    self->shift32 = 0;
    self->counter = 0;
    self->toggle = TOGGLE_SHIFT;
}

static inline uint32_t peek_counter_(struct fbp_pattern_32a_tx_s * self) {
    return (((uint32_t) ~self->counter) << 16) | self->counter;
}

static inline void advance_counter_(struct fbp_pattern_32a_tx_s * self) {
    ++self->counter;
}

static inline uint32_t peek_shift_(struct fbp_pattern_32a_tx_s * self) {
    return self->shift32;
}

static inline void advance_shift_(struct fbp_pattern_32a_tx_s * self) {
    self->shift32 = (0 == self->shift32) ? 1 : (self->shift32 << 1);
}

static inline uint32_t peek_(struct fbp_pattern_32a_tx_s * self) {
    uint32_t value;
    if (TOGGLE_SHIFT == self->toggle) {
        value = peek_shift_(self);
    } else {
        value = peek_counter_(self);
    }
    return value;
}

static inline void advance_(struct fbp_pattern_32a_tx_s * self) {
    if (TOGGLE_SHIFT == self->toggle) {
        advance_shift_(self);
        self->toggle = TOGGLE_COUNT;
    } else {
        advance_counter_(self);
        self->toggle = TOGGLE_SHIFT;
    }
}

uint32_t fbp_pattern_32a_tx_next(struct fbp_pattern_32a_tx_s * self) {
    uint32_t value;
    if (TOGGLE_SHIFT == self->toggle) {
        value = peek_shift_(self);
        advance_shift_(self);
        self->toggle = TOGGLE_COUNT;
    } else {
        value = peek_counter_(self);
        advance_counter_(self);
        self->toggle = TOGGLE_SHIFT;
    }
    return value;
}

void fbp_pattern_32a_tx_buffer(
        struct fbp_pattern_32a_tx_s * self,
        uint32_t * buffer,
        uint32_t size) {
    uint32_t sz = size / 4;
    uint32_t counter_offset = 1;
    uint32_t shift_offset = 0;
    if (TOGGLE_COUNT == self->toggle) {
        counter_offset = 0;
        shift_offset = 1;
    }
    for (uint32_t i = counter_offset; i < sz; i += 2) {
        buffer[i] = peek_counter_(self);
        advance_counter_(self);
    }
    for (uint32_t i = shift_offset; i < sz; i += 2) {
        buffer[i] = peek_shift_(self);
        advance_shift_(self);
    }
    if (sz & 1) {
        self->toggle = (TOGGLE_SHIFT == self->toggle) ? TOGGLE_COUNT : TOGGLE_SHIFT;
    }
}

enum rx_state_e {
    ST_UNSYNC,
    ST_UNSYNC2,
    ST_WORD2,
    ST_SYNC,
};

void fbp_pattern_32a_rx_initialize(
        struct fbp_pattern_32a_rx_s * self) {
    FBP_DBC_NOT_NULL(self);
    fbp_pattern_32a_tx_initialize(&self->tx);
    self->receive_count = 0;
    self->missing_count = 0;
    self->error_count = 0;
    self->resync_count = 0;
    self->syncword1 = 0;
    self->state = ST_UNSYNC;
}

static bool is_counter_value(uint32_t value) {
    return (((~value) >> 16) == (value & 0xffff));
}

static uint8_t shift_position(uint32_t shift) {
    // CLZ instruction would be much faster
    uint8_t position = 0;
    while (shift != 0) {
        shift = shift >> 1U;
        ++position;
    }
    return position;
}

/**
 * @brief Resync to the pattern.
 *
 * @param self The instance
 * @param v1 The older (first received) word.
 * @param v2 The newer (second received) word.
 * @return True if resync, false if could not resync.
 */
static bool resync(struct fbp_pattern_32a_rx_s * self, uint32_t v1, uint32_t v2) {
    struct fbp_pattern_32a_tx_s tx_old = self->tx;
    bool v1_is_counter = is_counter_value(v1);
    bool v2_is_counter = is_counter_value(v2);
    uint32_t incr = 0;
    if (v1_is_counter == v2_is_counter) {
        return false;
    }

    // Since tx state holds next values, sync to before v1
    if (v2_is_counter && !v1_is_counter) { // most recent is counter
        self->tx.shift32 = v1;
        self->tx.counter = v2 & 0xffff;
        self->tx.toggle = TOGGLE_SHIFT; // 2 values behind next expected
    } else if (!v2_is_counter && v1_is_counter) { // most recent is shift
        self->tx.counter = v1 & 0xffff;
        self->tx.shift32 = v2;
        self->tx.toggle = TOGGLE_COUNT; // 2 values behind next expected
    }

    // Must be on same subpattern to compare correctly
    if (tx_old.toggle != self->tx.toggle) {
        fbp_pattern_32a_tx_next(&tx_old);
        incr = 1;
    }

    // Determine closest pattern repeat
    uint32_t delta = ((uint32_t) (self->tx.counter - tx_old.counter)) & 0xffff;
    uint8_t sp_now = shift_position(self->tx.shift32);
    uint8_t sp_old = shift_position(tx_old.shift32);
    while (((sp_old + delta) % SHIFT_PERIOD) != sp_now) {
        delta += (1 << 16);
    }
    delta *= 2; // total pattern value = 2 words

    if (delta > (FBP_PATTERN_32A_PERIOD / 2)) {
        // presume duplicated
        self->duplicate_count = FBP_PATTERN_32A_PERIOD - delta - incr;
    } else {
        self->missing_count += incr + delta;
    }

    // Advance to next expected
    fbp_pattern_32a_tx_next(&self->tx);
    fbp_pattern_32a_tx_next(&self->tx);

    return true;
}

void fbp_pattern_32a_rx_next(
        struct fbp_pattern_32a_rx_s * self,
        uint32_t value) {
    switch (self->state) {
        case ST_UNSYNC:
            self->syncword1 = value;
            self->state = ST_UNSYNC2;
            break;
        case ST_UNSYNC2: {
            if (resync(self, self->syncword1, value)) {
                self->missing_count = 0;
                self->error_count = 0;
                self->state = ST_SYNC;
            } else {
                ++self->error_count;
                self->syncword1 = value;
                self->state = ST_WORD2;
            }
            break;
        }
        case ST_WORD2: {
            if (resync(self, self->syncword1, value)) {
                self->state = ST_SYNC;
            } else {
                ++self->error_count;
                self->syncword1 = value;
            }
            break;
        }
        case ST_SYNC: {
            if (value != peek_(&self->tx)) {
                ++self->resync_count;
                ++self->error_count;
                self->syncword1 = value;
                self->state = ST_WORD2;
            } else {
                advance_(&self->tx);
            }
            break;
        }
        default:
            FBP_FATAL("invalid state");
            self->state = ST_UNSYNC;
            break;
    }
    ++self->receive_count;
}

void fbp_pattern_32a_rx_buffer(
        struct fbp_pattern_32a_rx_s * self,
        uint32_t const * buffer,
        uint32_t size) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(buffer);
    uint32_t sz = size / 4;
    for (uint32_t i = 0; i < sz; ++i) {
        fbp_pattern_32a_rx_next(self, buffer[i]);
    }
}
