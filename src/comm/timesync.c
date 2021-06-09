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
 * This module implements a filter to estimate UTC.
 *
 * The relationship between the counter and UTC is:
 *
 *      t = scale * (counter + offset)
 *
 * However, the actual implementation must ensure that we maintain precision
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
 *
 */


#define UPDATE_COUNT (16)  // must be power of 2
#define UPDATE_PROCESS_MAX (12)
#define UPDATE_INDEX_MASK (UPDATE_COUNT - 1)
#define TIME_PROCESS_STD (10e-9)   // 10 ppb
#define SCALE_PROCESS_STD (10e-9)  // 10 ppb

#define FBP_TS_INITIAL_OFFSET_P ((double) UINT32_MAX)
#define FBP_TS_INITIAL_SCALE_P  ((double) 0.01)


struct update_s {
    uint64_t counter;    // The mean counter value = (start + stop) / 2
    uint64_t time;       // The mean time value = (rx + tx) / 2
    uint64_t dcounter;   // The estimated, one-way duration = (start - stop) / 2
};

struct fbp_ts_s {
    fbp_os_mutex_t mutex;

    // FIFO for incoming time updates
    struct update_s updates[UPDATE_COUNT];
    uint8_t update_head;
    uint8_t process_head;
    uint8_t process_tail;

    // Kalman filter state x[2] = [time, scale]
    // double time;    // in seconds, set to 0.0 using counter_offset and time_offset
    double scale;      // scale from counter to time, nominally 1.0
    double scale_nom;  // nominal scale, to keep filter normalized regardless of counter frequency
    // Kalman filter error covariance
    double p[2][2];

    // Offsets to keep Kalman filter computation near zero for numerical precision
    uint64_t counter_offset;    // in counter units
    int64_t time_offset;        // in fitterbap 34Q30 time
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

// Compute the offset-removed time
static inline double compute_time_f64a(uint64_t counter, uint64_t counter_offset, double scale, double scale_nom) {
    return (scale * scale_nom) * (double) ((int64_t) ((counter) - (counter_offset)));
}

static inline double compute_time_f64(struct fbp_ts_s * self, uint64_t counter) {
    return compute_time_f64a(counter, self->counter_offset, self->scale, self->scale_nom);
}

static inline int64_t compute_time_i64(struct fbp_ts_s * self, uint64_t counter) {
    double t = compute_time_f64a(counter, self->counter_offset, self->scale, self->scale_nom);
    return FBP_F64_TO_TIME(t);
}

FBP_API int64_t fbp_ts_time(struct fbp_ts_s * self) {
    self = resolve_instance(self);
    if (!self) {
        return 0;
    }

    // Thread-safety: get coefficients using mutex
    lock(self);
    uint64_t counter_offset = self->counter_offset;
    double scale = self->scale;
    int64_t time_offset = self->time_offset;
    unlock(self);

    // Get counter, may not always be instantaneous
    uint64_t counter = fbp_ts_counter();

    // Compute time
    double t = compute_time_f64a(counter, counter_offset, scale, self->scale_nom);
    return FBP_F64_TO_TIME(t) + time_offset;
}

static inline uint8_t next_idx(uint8_t idx) {
    return (idx + 1) & UPDATE_INDEX_MASK;
}

static inline uint8_t prev_idx(uint8_t idx) {
    return (idx - 1) & UPDATE_INDEX_MASK;
}

/**
 * @brief Multiply two matrices.
 *
 * @param r The result matrix.
 * @param m The first input matrix.
 * @param n The second input matrix.
 */
static void matrix_mult_2x2(double r[2][2], double m[2][2], double n[2][2]) {
    double a = m[0][0];
    double b = m[0][1];
    double c = m[1][0];
    double d = m[1][1];
    double w = n[0][0];
    double x = n[0][1];
    double y = n[1][0];
    double z = n[1][1];
    r[0][0] = a * w + b * y;
    r[0][1] = a * x + b * z;
    r[1][0] = c * w + d * y;
    r[1][1] = c * x + d * z;
}

static void matrix_inv_2x2(double m[2][2]) {
    double a = m[0][0];
    double b = m[0][1];
    double c = m[1][0];
    double d = m[1][1];
    double s = 1.0 / (a * d - b * c);
    m[0][0] = s * d;
    m[0][1] = s * -b;
    m[1][0] = s * -c;
    m[1][1] = s * a;
}

static int32_t process_one(struct fbp_ts_s * self) {
    struct update_s * prior = &self->updates[self->process_tail];
    struct update_s * current = &self->updates[self->process_head];
    struct update_s * prev = &self->updates[prev_idx(self->process_head)];

    if (self->process_head == self->update_head) {
        return 1;  // no work to do
    }

    if (self->process_tail == self->process_head) {
        // initial update since tail should eventually be UPDATE_PROCESS_MAX behind.
        self->counter_offset = current->counter;
        self->time_offset = current->time;
        self->scale = 1.0;
        self->scale_nom = 1.0 / fbp_time_counter().frequency;
        self->process_head = next_idx(self->process_head);
        return 0;
    }

    // Project the state ahead
    double t_e = compute_time_f64(self, current->counter);
    double s_e = self->scale;

    // Validate that Kalman filter is tracking
    int64_t t_e_i64 = FBP_F64_TO_TIME(t_e);
    int64_t t_e_err = (current->time - self->time_offset ) - t_e_i64;
    if ((t_e_err > FBP_TIME_SECOND) || (t_e_err < -FBP_TIME_SECOND)) {
        FBP_LOGW("timesync error - force resync");
        self->process_tail = self->process_head;
        return 0;  // force reset to initial update
    }

    // project the error covariance ahead from previous update
    int64_t dt1_i64 = (current->time - prev->time);
    double dt1 = FBP_TIME_TO_F64(dt1_i64);
    double dc1 = (double) ((int64_t) (current->counter - prev->counter));
    double p00 = self->p[0][0];
    double p01 = self->p[0][1];
    double p10 = self->p[1][0];
    double p11 = self->p[1][1];
    double dz = p11 * dc1;
    self->p[0][0] = p00 + (p10 + p10 + dz) * dc1;
    self->p[0][1] = p01 + dz;
    self->p[1][0] = p10 + dz;
    self->p[1][1] = p11;

    // estimate and add the process error to projected covariance, from last update time
    self->p[0][0] += dt1 * (TIME_PROCESS_STD * TIME_PROCESS_STD);
    self->p[1][1] += dt1 * (SCALE_PROCESS_STD * SCALE_PROCESS_STD);

    // Compute the measured values
    int64_t dt_i64 = (current->time - prior->time);
    double dt = FBP_TIME_TO_F64(dt_i64);
    double dc = (double) ((int64_t) (current->counter - prior->counter));
    double t_m = FBP_TIME_TO_F64(current->time - self->time_offset);
    double s_m = dt / (dc * self->scale_nom);

    // estimate the measurement error and compute Kalman gain
    double r[2][2];
    double k[2][2];
    double t_r = (self->scale * self->scale_nom) * current->dcounter;
    double s_r = ((self->scale * self->scale_nom) * (prior->dcounter + current->dcounter)) / dt;
    r[0][0] = self->p[0][0] + t_r * t_r;
    r[0][1] = self->p[0][1];
    r[1][0] = self->p[1][0];
    r[1][1] = self->p[1][1] + s_r * s_r;
    matrix_inv_2x2(r);
    matrix_mult_2x2(k, self->p, r);

    // update state estimate using the measurement
    double t_zx = t_m - t_e;
    double s_zx = s_m - s_e;
    t_e += k[0][0] * t_zx + k[0][1] * s_zx;
    s_e += k[1][0] * t_zx + k[1][1] * s_zx;

    // update the error covariance
    k[0][0] = 1 - k[0][0];
    k[0][1] = -k[0][1];
    k[1][0] = -k[1][0];
    k[1][1] = 1 - k[1][1];
    matrix_mult_2x2(self->p, k, self->p);

    // update the state (used for fbp_ts_time() computations).
    int64_t time_offset_next = self->time_offset + FBP_F64_TO_TIME(t_e);
    int64_t counter_incr = (int64_t) (t_e / (self->scale * self->scale_nom));
    uint64_t counter_offset_next = (uint64_t) (self->counter_offset + counter_incr);
    lock(self);
    self->scale = s_e;
    self->time_offset = time_offset_next;
    self->counter_offset = counter_offset_next;
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
    current_entry->counter = (src_tx >> 1) + (src_rx >> 1);
    current_entry->time = (tgt_tx >> 1) + (tgt_rx >> 1);
    current_entry->dcounter = ((src_rx - src_tx) >> 1);
    self->update_head = next_update_head;

    fbp_ts_process(self);
}

FBP_API struct fbp_ts_s * fbp_ts_initialize() {
    struct fbp_ts_s * self = fbp_alloc_clr(sizeof(struct fbp_ts_s));
    self->mutex = fbp_os_mutex_alloc();
    self->scale = 1.0;
    self->scale_nom = 1.0 / fbp_time_counter().frequency;
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
