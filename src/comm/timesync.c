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

#include "fitterbap/comm/timesync.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/os/mutex.h"


/**
 * This module converts a local counter into UTC time based upon
 * external updates.
 *
 * The relationship between the counter and UTC is:
 *
 *      t_utc = counter_period * (counter_value - counter_offset)
 *
 * We can introduce an optional t_offset to help manage numerical precision:
 *
 *      t = t_utc - t_offset
 *
 * The existing implementation is trivial.  When used at the default 10 second
 * update rate from the port0 implementation, it estimates period over
 * overlapping 120 second windows, with on update every 10 seconds.  At each
 * update, it sets the time to exactly the reported time.  This implementation
 * is similar to SNTP, and it performs no additional filtering, which means
 * that your system will observe small, frequent jumps.
 *
 * However, more accurate filtering is possible.  See some of the references
 * below.  the actual implementation must ensure that we maintain precision
 * across the full range of possible counter frequencies.  Any implementation
 * must carefully analyze numerical precision to ensure sufficient accuracy.
 *
 * How accurate?  If we want to allow for 1 µs accuracy over 100 seconds,
 * then we need to represent 10 ns error per second, which requires at least
 * log2(10e-9) = 27 bits.
 *
 *
 * ## References
 *
 * - [An Introduction to the Kalman Filter](https://www.cs.unc.edu/~welch/media/pdf/kalman_intro.pdf),
 *   Greg Welch & Gary Bishop.
 * - [ACES: Adaptive Clock Estimation and Synchronization Using Kalman Filtering](
 *   https://www.cc.gatech.edu/home/jx/reprints/Hamilton et al. - 2008 - ACES Adaptive Clock Estimation and Synchronizatio.pdf)
 *   Hamilton, Zhao, Ma, Xu.
 * - Simple Network Time Protocol (SNTP)
 *   - (RFC 4330](https://datatracker.ietf.org/doc/html/rfc4330)
 * - Network Time Protocol (NTP)
 *   - [RFC 5905](https://datatracker.ietf.org/doc/html/rfc5905)
 *   - [Wikipedia](https://en.wikipedia.org/wiki/Network_Time_Protocol)
 *   - [How does it work?](http://www.ntp.org/ntpfaq/NTP-s-algo.htm).
 * - Precision Time Protocol (PTP)
 *   - [Wikipedia](https://en.wikipedia.org/wiki/Precision_Time_Protocol)
 *   - [IEEE 1588-2019](https://ieeexplore.ieee.org/document/9120376)
 */


#define FREQ_MAX (2000000)
#define UPDATE_COUNT (16)  // must be power of 2
#define UPDATE_PROCESS_MAX (12)
#define UPDATE_INDEX_MASK (UPDATE_COUNT - 1)

struct update_s {
    uint64_t counter;    // The mean counter value = ((start + stop) / 2) >> counter_right_shift
    uint64_t time;       // The mean time value = (rx + tx) / 2
    uint64_t dcounter;   // The total counter duration = (start - stop) >> counter_right_shift
};

struct fbp_ts_s {
    fbp_os_mutex_t mutex;

    // FIFO for incoming time updates
    struct update_s updates[UPDATE_COUNT];
    uint8_t update_head;
    uint8_t process_head;
    uint8_t process_tail;

    uint8_t counter_right_shift;
    uint64_t counter_offset;        // in counter units shifted by counter_right_shift
    int64_t time_offset;            // in fitterbap 34Q30 time
    uint64_t counter_period_12q52;
};

struct fbp_ts_s * primary_instance_ = NULL;


static inline struct fbp_ts_s * resolve_instance(struct fbp_ts_s * self) {
    if (!self) {
        if (!primary_instance_) {
            return NULL;
        }
        self = primary_instance_;
    }
    return self;
}

static inline void lock(struct fbp_ts_s * self) {
    fbp_os_mutex_lock(self->mutex);
}

static inline void unlock(struct fbp_ts_s * self) {
    fbp_os_mutex_unlock(self->mutex);
}

FBP_API int64_t fbp_ts_time(struct fbp_ts_s * self) {
    self = resolve_instance(self);
    if (!self) {
        return 0;
    }

    // Thread-safety: get coefficients using mutex
    lock(self);
    uint64_t counter_period_12q52 = self->counter_period_12q52;
    uint64_t counter_offset = self->counter_offset;
    int64_t time_offset = self->time_offset;
    unlock(self);

    // Get counter, may not always be instantaneous
    int64_t counter = fbp_time_counter_u64() >> self->counter_right_shift;
    counter -= counter_offset;
    int64_t value = counter_period_12q52 * counter;
    value = (value >> 22) + time_offset;
    return value;
}

static inline uint8_t next_idx(uint8_t idx) {
    return (idx + 1) & UPDATE_INDEX_MASK;
}

static inline uint8_t prev_idx(uint8_t idx) {
    return (idx - 1) & UPDATE_INDEX_MASK;
}

static int32_t process_one(struct fbp_ts_s * self) {
    struct update_s * prior = &self->updates[self->process_tail];
    struct update_s * current = &self->updates[self->process_head];
    // struct update_s * prev = &self->updates[prev_idx(self->process_head)];

    if (self->process_head == self->update_head) {
        return 1;  // no work to do
    }

    if (self->process_tail == self->process_head) {
        // initial update since tail should eventually be UPDATE_PROCESS_MAX behind.
        self->counter_offset = current->counter;
        self->time_offset = current->time;
        self->process_head = next_idx(self->process_head);
        return 0;
    }

    if ((prior->time > current->time) || (prior->counter > current->counter)) {
        FBP_LOGW("Receive past event, force resync");
        self->process_tail = self->process_head;
        return 0;
    }

    // Compute the measured values
    uint64_t dt_u64 = (uint64_t) (current->time - prior->time);  // 34Q30
    uint64_t dc_u64 = (uint64_t) (current->counter - prior->counter);  // already right-shifted
    if (dc_u64 > UINT32_MAX) {
        FBP_LOGW("counter interval exceeds 32-bits, resync");
        self->process_tail = self->process_head;
        return 0;
    }
    if (dt_u64 > (FBP_TIME_SECOND << 10)) {
        FBP_LOGW("duration exceeds 1024 seconds, resync");
        self->process_tail = self->process_head;
        return 0;
    }
    uint64_t counter_period_12q52 = (dt_u64 << 22 /* 12Q52 */) / (uint32_t) dc_u64;

    // update the state (used for fbp_ts_time() computations).
    lock(self);
    self->counter_period_12q52 = counter_period_12q52;
    self->time_offset = current->time;
    self->counter_offset = current->counter;
    unlock(self);

    // Advance the process index pointers.
    self->process_head = next_idx(self->process_head);
    while (1) {
        uint8_t idx_delta = (self->process_head - self->process_tail) & UPDATE_INDEX_MASK;
        if (idx_delta > UPDATE_PROCESS_MAX) {
            self->process_tail = next_idx(self->process_tail);
        } else {
            break;
        }
    }

    return 0;
}

static int32_t fbp_ts_process(struct fbp_ts_s * self) {
    while (0 == process_one(self)) {
        // continue;
    }
    return 0;
}

FBP_API void fbp_ts_update(struct fbp_ts_s * self, uint64_t src_tx, int64_t tgt_rx, int64_t tgt_tx, uint64_t src_rx) {
    if (!self) {
        return;  // ignore
    }
    if (!tgt_rx || !tgt_tx) {
        return;  // target does not know UTC time, ignore
    }
    if (src_tx > src_rx) {
        return;  // invalid counter, not monotonic
    }
    if (tgt_rx > tgt_tx) {
        return;  // invalid UTC, not increasing
    }

    uint8_t next_update_head = next_idx(self->update_head);
    if (next_update_head == self->process_tail) {
        FBP_LOGE("update head caught up to process tail");
        self->process_tail = next_idx(self->process_tail);
    }

    struct update_s *current_entry = &self->updates[self->update_head];
    current_entry->counter = ((src_tx >> 1) + (src_rx >> 1)) >> self->counter_right_shift;
    current_entry->time = (tgt_tx >> 1) + (tgt_rx >> 1);
    current_entry->dcounter = ((src_rx - src_tx) >> self->counter_right_shift);
    self->update_head = next_update_head;

    fbp_ts_process(self);
}

FBP_API struct fbp_ts_s * fbp_ts_initialize() {
    struct fbp_ts_s * self = fbp_alloc_clr(sizeof(struct fbp_ts_s));
    self->mutex = fbp_os_mutex_alloc("fbp_ts");
    uint32_t frequency = fbp_time_counter_frequency();
    while (frequency > FREQ_MAX) {
        ++self->counter_right_shift;
        frequency >>= 1;
    }
    self->counter_period_12q52 = (((uint64_t) 1) << 52) / frequency;

    if (!primary_instance_) {
        primary_instance_ = self;
    }
    return self;
}

FBP_API void fbp_ts_finalize(struct fbp_ts_s * self) {
    if (self) {
        fbp_os_mutex_t mutex = self->mutex;
        self->mutex = NULL;
        if (mutex) {
            fbp_os_mutex_lock(mutex);
        }
        if (self == primary_instance_) {
            primary_instance_ = NULL;
        }
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
            fbp_os_mutex_free(mutex);
        }
    }
}
