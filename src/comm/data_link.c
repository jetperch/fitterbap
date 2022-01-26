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

/*
 * Known issues:
 *
 * - 1: Possible increased latency on corrupted length.
 *   If the receiver receives a frame with a length corrupted to a larger
 *   payload, then the framer will buffer retries until it fills the
 *   buffer as if it was processing the larger corrupted length frame.
 *
 *   In the worst case, the smallest possible frame length is corrupted
 *   to the largest possible frame length.  The number of retries
 *   required to complete the length and then transmit the frame will be:
 *   (268 - 8) / (12 + 1 + 1) + 1 = 20 retries.
 *
 *   Potential workarounds to speed link flush:
 *   - send a full-frame worth of SOF when transmitter is idle.
 *   - If detect consecutive retries on same message with no other activity,
 *     then send a full-frame worth of SOF
 */

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE
#include "fitterbap/comm/data_link.h"
#include "fitterbap/collections/ring_buffer_u64.h"
#include "fitterbap/os/task.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include <inttypes.h>

/**
 * @brief The maximum number of attempts to send a message before reseting the link.
 *
 * This value must be large enough to transmit a full-sized frame worth of
 * data given a smallest-sized frame.
 */
#define SEND_COUNT_MAX (25)


enum state_e {
    ST_DISCONNECTED,
    ST_CONNECTED,
};

enum tx_frame_state_e {
    TX_FRAME_ST_IDLE        = 0,
    TX_FRAME_ST_VALID       = (1 << 0),
    TX_FRAME_ST_ACK         = (1 << 1),
    TX_FRAME_ST_FORCE       = (1 << 2),
};

enum rx_frame_state_e {
    RX_FRAME_ST_IDLE,
    RX_FRAME_ST_ACK,
    RX_FRAME_ST_NACK,
};

struct tx_frame_s {
    uint8_t state;
    uint8_t send_count;
    uint16_t length;
    int64_t next_send_time;
    uint8_t msg[FBP_FRAMER_MAX_SIZE];
};

struct rx_frame_s {
    uint8_t state;
    uint8_t msg_size;  // minus 1
    uint16_t metadata;
    uint8_t msg[FBP_FRAMER_PAYLOAD_MAX_SIZE];
};

struct fbp_dl_s {
    struct fbp_dl_ll_s ll_instance;
    uint32_t ll_size_max;     // at init, when empty
    struct fbp_dl_api_s ul_instance;
    struct fbp_framer_s * framer;

    uint16_t tx_frame_last_id; // the last frame that has not yet be ACKed
    uint16_t tx_frame_next_id; // the next frame id for sending.
    uint16_t rx_frame_next_id; // the next frame that has not yet been received
    uint16_t rx_frame_max_id;  // the most future stored frame id

    struct fbp_rbu64_s tx_link_buf;

    struct tx_frame_s * tx_frames;
    uint16_t tx_frame_count;
    uint16_t tx_frame_count_max;
    int64_t tx_timeout;
    struct rx_frame_s * rx_frames;
    uint16_t rx_frame_count;
    uint8_t tx_eof_pending;

    enum state_e state;
    int64_t tx_reset_next;

    fbp_os_mutex_t mutex;
    fbp_dl_process_request_fn process_request_fn;
    void * process_request_user_data;

    struct fbp_dl_rx_status_s rx_status;
    struct fbp_dl_tx_status_s tx_status;
};

static inline void lock(struct fbp_dl_s * self) {
    if (self->mutex) {
        fbp_os_mutex_lock(self->mutex);
    }
}

static inline void unlock(struct fbp_dl_s * self) {
    if (self->mutex) {
        fbp_os_mutex_unlock(self->mutex);
    }
}

int32_t fbp_dl_send(struct fbp_dl_s * self,
                    uint16_t metadata,
                    uint8_t const *msg, uint32_t msg_size) {
    if (self->state != ST_CONNECTED) {
        return FBP_ERROR_UNAVAILABLE;
    }
    struct fbp_framer_s * framer = self->framer;
    if (msg_size > FBP_FRAMER_PAYLOAD_MAX_SIZE) {
        FBP_LOGW("fbp_framer_send msg_size too big: %d", (int) msg_size);
        return FBP_ERROR_PARAMETER_INVALID;
    }
    lock(self);
    uint16_t frame_id = self->tx_frame_next_id;
    uint16_t idx = frame_id & (self->tx_frame_count - 1);
    struct tx_frame_s * f = &self->tx_frames[idx];
    if (f->state != TX_FRAME_ST_IDLE) {
        unlock(self);
        FBP_LOGD1("fbp_dl_send(0x%02" PRIx16 ") when full", metadata);
        return FBP_ERROR_FULL;
    }

    f->send_count = 0;
    f->length = sizeof(f->msg);
    f->next_send_time = 0;
    self->tx_status.msg_bytes += msg_size;

    FBP_ASSERT(0 == framer->construct_data(framer, f->msg, &f->length, frame_id, metadata, msg, msg_size));
    self->tx_frame_next_id = (frame_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
    f->state = TX_FRAME_ST_VALID | TX_FRAME_ST_FORCE;
    unlock(self);

    if (self->process_request_fn) {
        self->process_request_fn(self->process_request_user_data);
    }

    return 0;
}

FBP_USED static uint16_t tx_buf_frame_id(struct tx_frame_s * f) {
    return (((uint16_t) f->msg[2] & 0x7) << 8) | f->msg[3];
}

static inline void event_emit(struct fbp_dl_s * self, enum fbp_dl_event_e event) {
    if (self->ul_instance.event_fn) {
        self->ul_instance.event_fn(self->ul_instance.user_data, event);
    }
}

static inline void send_ll(struct fbp_dl_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    self->tx_status.bytes += buffer_size;
    self->ll_instance.send(self->ll_instance.user_data, buffer, buffer_size);
}

static int send_link_pending(struct fbp_dl_s * self) {
    uint32_t pending_sz = fbp_rbu64_size(&self->tx_link_buf);
    if (!pending_sz) {
        return 0;
    }
    uint32_t send_sz = self->ll_instance.send_available(self->ll_instance.user_data) / FBP_FRAMER_LINK_SIZE;
    if (!send_sz) {
        FBP_LOGD1("send_link_pending send_available = 0");
        return 1;
    } else if (pending_sz <= send_sz) {
        send_sz = pending_sz;
    }
    if ((self->tx_link_buf.tail + send_sz) > self->tx_link_buf.buf_size) {
        // wrap around, send in two parts
        uint32_t sz = self->tx_link_buf.buf_size - self->tx_link_buf.tail;
        send_ll(self, (uint8_t *) fbp_rbu64_tail(&self->tx_link_buf), sz * FBP_FRAMER_LINK_SIZE);
        fbp_rbu64_discard(&self->tx_link_buf, sz);
        send_sz -= sz;
        send_ll(self, (uint8_t *) self->tx_link_buf.buf, send_sz * FBP_FRAMER_LINK_SIZE);
        fbp_rbu64_discard(&self->tx_link_buf, send_sz);
    } else {
        send_ll(self, (uint8_t *) fbp_rbu64_tail(&self->tx_link_buf), send_sz * FBP_FRAMER_LINK_SIZE);
        fbp_rbu64_discard(&self->tx_link_buf, send_sz);
    }
    self->tx_eof_pending = 1;
    return 0;
}

static void send_link(struct fbp_dl_s * self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    struct fbp_framer_s * framer = self->framer;
    uint64_t b;
    int32_t rv = framer->construct_link(framer, &b, frame_type, frame_id);
    if (rv) {
        FBP_LOGW("send_link error: %d", (int) rv);
        return;
    }
    if (!fbp_rbu64_push(&self->tx_link_buf, b)) {
        FBP_LOGW("link buffer full %d %d", (int) frame_type, (int) frame_id);
    }
}

static inline int64_t reset_timeout_duration(struct fbp_dl_s * self) {
    return self->tx_timeout * 16;
}

static void reset_state(struct fbp_dl_s * self) {
    FBP_LOGD1("reset_state");

    // tx direction
    self->tx_frame_last_id = 0;
    self->tx_frame_next_id = 0;
    for (uint16_t f = 0; f < self->tx_frame_count_max; ++f) {
        self->tx_frames[f].state = TX_FRAME_ST_IDLE;
    }
    self->tx_frame_count = 1;  // decrease window size, need to negotiate larger

    // rx direction
    self->rx_frame_next_id = 0;
    self->rx_frame_max_id = 0;
    fbp_rbu64_clear(&self->tx_link_buf);
    for (uint16_t f = 0; f < self->rx_frame_count; ++f) {
        self->rx_frames[f].state = RX_FRAME_ST_IDLE;
    }
    self->tx_reset_next = FBP_TIME_MAX;  // assign in process_disconnected

    if (self->state != ST_DISCONNECTED) {
        self->state = ST_DISCONNECTED;
        event_emit(self, FBP_DL_EV_DISCONNECTED);
    }
}

static inline void send_eof(struct fbp_dl_s * self) {
    uint8_t eof[] = {FBP_FRAMER_SOF1};
    send_ll(self, eof, sizeof(eof));
    self->tx_eof_pending = 0;
}

static void on_recv_msg_done(struct fbp_dl_s * self, uint16_t metadata, uint8_t *msg, uint32_t msg_size) {
    if (self->ul_instance.recv_fn) {
        self->ul_instance.recv_fn(self->ul_instance.user_data, metadata, msg, msg_size);
    }
    self->rx_status.msg_bytes += msg_size;
    ++self->rx_status.data_frames;
}

static void on_recv_data(void * user_data, uint16_t frame_id, uint16_t metadata,
                         uint8_t *msg, uint32_t msg_size) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) user_data;
    uint16_t this_idx = frame_id & (self->rx_frame_count - 1U);
    uint16_t window_end = (self->rx_frame_next_id + self->rx_frame_count) & FBP_FRAMER_FRAME_ID_MAX;

    FBP_ASSERT(frame_id == (frame_id & FBP_FRAMER_FRAME_ID_MAX));  // otherwise framer is broken
    FBP_ASSERT((0 != msg_size) && (msg_size <= FBP_FRAMER_PAYLOAD_MAX_SIZE));

    if (self->rx_frame_next_id == frame_id) {
        // next expected frame, recv immediately without putting into ring buffer.
        self->rx_frames[this_idx].state = RX_FRAME_ST_IDLE;
        on_recv_msg_done(self, metadata, msg, msg_size);
        self->rx_frame_next_id = (self->rx_frame_next_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
        if (self->rx_frame_max_id == frame_id) {
            FBP_LOGD2("on_recv_data(%d), ACK_ALL", (int) frame_id);
            self->rx_frame_max_id = self->rx_frame_next_id;
            send_link(self, FBP_FRAMER_FT_ACK_ALL, frame_id);
        } else {
            while (1) {
                this_idx = self->rx_frame_next_id & (self->rx_frame_count - 1U);
                struct rx_frame_s * f = &self->rx_frames[this_idx];
                if (f->state != RX_FRAME_ST_ACK) {
                    break;
                }
                FBP_LOGD2("on_recv_data(%d), catch up", (int) self->rx_frame_next_id);
                f->state = RX_FRAME_ST_IDLE;
                on_recv_msg_done(self, f->metadata, f->msg, 1 + (uint16_t) f->msg_size);
                self->rx_frame_next_id = (self->rx_frame_next_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
            }
            frame_id = (self->rx_frame_next_id - 1U) & FBP_FRAMER_FRAME_ID_MAX;
            send_link(self, FBP_FRAMER_FT_ACK_ALL, frame_id);
        }
    } else if (fbp_dl_frame_id_subtract(frame_id, self->rx_frame_next_id) < 0) {
        // we already have this frame.
        // ack all with most recent successfully received frame_id
        FBP_LOGD3("on_recv_data(%d) old frame next=%d", (int) frame_id, (int) self->rx_frame_next_id);
        send_link(self, FBP_FRAMER_FT_ACK_ALL, (self->rx_frame_next_id - 1) & FBP_FRAMER_FRAME_ID_MAX);
    } else if (fbp_dl_frame_id_subtract(window_end, frame_id) <= 0) {
        FBP_LOGD1("on_recv_data(%d) frame too far into the future: next=%d, end=%d",
                  (int) frame_id, (int) self->rx_frame_next_id, (int) window_end);
        send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, frame_id);
    } else {
        FBP_LOGD3("on_recv_data(%d) future frame: next=%d, end=%d",
                  (int) frame_id, (int) self->rx_frame_next_id, (int) window_end);
        // future frame
        if (fbp_dl_frame_id_subtract(frame_id, self->rx_frame_max_id) > 0) {
            self->rx_frame_max_id = frame_id;
        }

        // nack missing frames not already NACK'ed
        uint16_t next_frame_id = self->rx_frame_next_id;
        while (1) {
            if (next_frame_id == frame_id) {
                break;
            }
            uint16_t next_idx = next_frame_id & (self->rx_frame_count - 1U);
            if (self->rx_frames[next_idx].state == RX_FRAME_ST_IDLE) {
                self->rx_frames[next_idx].state = RX_FRAME_ST_NACK;
                send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, next_frame_id);
            }
            next_frame_id = (next_frame_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
        }

        // store
        self->rx_frames[this_idx].state = RX_FRAME_ST_ACK;
        self->rx_frames[this_idx].msg_size = msg_size - 1;
        self->rx_frames[this_idx].metadata = metadata;
        fbp_memcpy(self->rx_frames[this_idx].msg, msg, msg_size);
        send_link(self, FBP_FRAMER_FT_ACK_ONE, frame_id);
    }
}

static struct tx_frame_s * tx_frame_get(struct fbp_dl_s * self, uint16_t frame_id) {
    uint16_t d_tail = (frame_id - self->tx_frame_last_id) & FBP_FRAMER_FRAME_ID_MAX;
    uint16_t d_head = (self->tx_frame_next_id - (frame_id + 1)) & FBP_FRAMER_FRAME_ID_MAX;
    if ((d_tail < self->tx_frame_count) && (d_head < self->tx_frame_count)) {
        uint16_t idx = frame_id & (self->tx_frame_count - 1);
        struct tx_frame_s *f = &self->tx_frames[idx];
        return f;
    } else {
        return NULL;
    }
}

static inline void retire_tx_frame_inner(struct fbp_dl_s * self, struct tx_frame_s * f) {
    self->tx_frame_last_id = (self->tx_frame_last_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
    ++self->tx_status.data_frames;
    f->state = TX_FRAME_ST_IDLE;
}

static bool retire_tx_frame(struct fbp_dl_s * self) {
    struct tx_frame_s * f = tx_frame_get(self, self->tx_frame_last_id);
    if (!f) {
        FBP_LOGW("retire_tx_frame not found valid: frame_id=%d", self->tx_frame_last_id);
        return false;
    } else if (f && (f->state & TX_FRAME_ST_VALID)) {
        retire_tx_frame_inner(self, f);
        return true;
    } else {
        FBP_LOGW("retire_tx_frame not valid: frame_id=%d, state=0x%02x", self->tx_frame_last_id, f->state);
        return false;
    }
}

static void handle_ack_all(struct fbp_dl_s * self, uint16_t frame_id) {
    while (1) {
        uint16_t frame_id_delta = (frame_id - self->tx_frame_last_id) & FBP_FRAMER_FRAME_ID_MAX;
        if (frame_id_delta < self->tx_frame_count) {
            if (!retire_tx_frame(self)) {
                break;
            }
        } else {
            break;
        }
    }
}

static void handle_ack_one(struct fbp_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s *f = tx_frame_get(self, frame_id);
    if (!f) {
        return;  // out of range
    } else if (f->state & TX_FRAME_ST_VALID) {
        f->state = TX_FRAME_ST_VALID | TX_FRAME_ST_ACK;
    }
    while (1) {
        f = tx_frame_get(self, self->tx_frame_last_id);
        if (f && ((f->state & (TX_FRAME_ST_VALID | TX_FRAME_ST_ACK)) == (TX_FRAME_ST_VALID | TX_FRAME_ST_ACK))) {
            retire_tx_frame_inner(self, f);
        } else {
            break;
        }
    }
}

static void handle_nack_frame_id(struct fbp_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s * f = tx_frame_get(self, frame_id);
    if (f && (f->state & TX_FRAME_ST_VALID)) {
        FBP_LOGD2("handle_nack_frame_id(%d)", (int) frame_id);
        f->state = TX_FRAME_ST_VALID | TX_FRAME_ST_FORCE;
    }
}

static void handle_nack_framing_error(struct fbp_dl_s * self, uint16_t frame_id) {
    struct tx_frame_s * f = tx_frame_get(self, frame_id);
    if (f && (f->state &= TX_FRAME_ST_VALID)) {
        FBP_LOGD2("handle_nack_framing_error(%d)", (int) frame_id);
        f->state = TX_FRAME_ST_VALID | TX_FRAME_ST_FORCE;
    }
}

static void handle_reset(struct fbp_dl_s * self, uint16_t frame_id) {
    FBP_LOGD2("received reset %d from remote host", (int) frame_id);
    switch (frame_id) {
        case 0:  // reset request
            if (self->state == ST_CONNECTED) {
                self->state = ST_DISCONNECTED;
                event_emit(self, FBP_DL_EV_RESET_REQUEST);
                reset_state(self);
                send_link(self, FBP_FRAMER_FT_RESET, 0);
                send_link(self, FBP_FRAMER_FT_RESET, 1);
            } else {
                send_link(self, FBP_FRAMER_FT_RESET, 1);
                self->state = ST_CONNECTED;
                event_emit(self, FBP_DL_EV_CONNECTED);
            }
            break;
        case 1:  // reset response
            if (self->state == ST_DISCONNECTED) {
                self->state = ST_CONNECTED;
                event_emit(self, FBP_DL_EV_CONNECTED);
            }
            break;
        default:
            FBP_LOGW("unsupported reset %d", (int) frame_id);
            return;
    }
}

static void on_recv_link(void * user_data, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) user_data;
    switch (frame_type) {
        case FBP_FRAMER_FT_ACK_ALL: handle_ack_all(self, frame_id); break;
        case FBP_FRAMER_FT_ACK_ONE: handle_ack_one(self, frame_id); break;
        case FBP_FRAMER_FT_NACK_FRAME_ID: handle_nack_frame_id(self, frame_id); break;
        case FBP_FRAMER_FT_NACK_FRAMING_ERROR: handle_nack_framing_error(self, frame_id); break;
        case FBP_FRAMER_FT_RESET: handle_reset(self, frame_id); break;
        default: break;
    }
}

static void on_framing_error(void * user_data) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) user_data;
    send_link(self, FBP_FRAMER_FT_NACK_FRAMING_ERROR, self->rx_frame_next_id);
}

void fbp_dl_ll_recv(struct fbp_dl_s * self,
                     uint8_t const * buffer, uint32_t buffer_size) {
    struct fbp_framer_s * framer = self->framer;
    framer->recv(framer, buffer, buffer_size);
}

static int64_t process_disconnected(struct fbp_dl_s * self, int64_t now) {
    if (self->tx_reset_next == FBP_TIME_MAX) {
        self->tx_reset_next = now + reset_timeout_duration(self);
    }
    if (fbp_rbu64_is_empty(&self->tx_link_buf) && (self->tx_reset_next <= now)) {
        send_link(self, FBP_FRAMER_FT_RESET, 0);
        self->tx_reset_next = now + reset_timeout_duration(self);
    }
    send_link_pending(self);
    if (self->tx_eof_pending && self->ll_instance.send_available(self->ll_instance.user_data)) {
        send_eof(self);
    }
    return fbp_time_max(self->tx_reset_next, now + FBP_TIME_MILLISECOND);
}

static inline int64_t tmin(int64_t a, int64_t b) {
    return (a < b) ? a : b;
}

static int64_t tx_transmit(struct fbp_dl_s * self, int64_t now) {
    uint32_t send_sz = self->ll_instance.send_available(self->ll_instance.user_data);
    int64_t next = INT64_MAX;
    uint16_t frame_id = self->tx_frame_last_id;
    for (uint16_t offset = 0; offset < self->tx_frame_count; ++offset) {
        // always start from oldest outstanding frame, check for retransmit opportunity
        uint16_t idx = frame_id & (self->tx_frame_count - 1);
        struct tx_frame_s * f = &self->tx_frames[idx];
        if (f->state == TX_FRAME_ST_IDLE) {
            break;
        }
        if (send_sz < FBP_FRAMER_MAX_SIZE) {
            // need to wait for fbp_dl_ll_send_done
            return next;
        }
        uint8_t state = f->state;
        if (TX_FRAME_ST_VALID == (state & (TX_FRAME_ST_VALID | TX_FRAME_ST_ACK))) {
            if ((state & TX_FRAME_ST_FORCE) || (now >= f->next_send_time)) {  // valid, no ack
                f->state = state & ~TX_FRAME_ST_FORCE;
                f->next_send_time = now + self->tx_timeout;
                if (f->send_count++) {
                    ++self->tx_status.retransmissions;
                }
                if (f->send_count > SEND_COUNT_MAX) {
                    FBP_LOGW("too many retries, reset");
                    reset_state(self);
                    next = now;
                    break;
                } else {
                    FBP_LOGD3("send_data(%d) buf->%d, count=%d, last=%d, next=%d",
                              (int) frame_id, (int) tx_buf_frame_id(f), (int) f->send_count,
                              (int) self->tx_frame_last_id, (int) self->tx_frame_next_id);
                    send_ll(self, f->msg, f->length);
                    send_sz -= f->length;
                    self->tx_eof_pending = 1;
                }
            }
            next = tmin(next, f->next_send_time);
        }
        frame_id = (frame_id + 1) & FBP_FRAMER_FRAME_ID_MAX;
    }
    if (self->tx_eof_pending && send_sz) {
        send_eof(self);
    }
    return next;
}

int64_t fbp_dl_process(struct fbp_dl_s * self, int64_t now) {
    int64_t next_event = INT64_MAX;
    if (self->state == ST_DISCONNECTED) {
        next_event = process_disconnected(self, now);
    } else if (send_link_pending(self)) {
        // lower-level is full, wait for next call to process
        return INT64_MAX;
    } else {
        next_event = tx_transmit(self, now);
    }
    FBP_ASSERT(next_event >= now);
    return next_event;
}

static uint32_t to_power_of_two(uint32_t v) {
    if (v == 0) {
        return v;
    }
    if (v > (1U << 31)) {
        // round down, which is ok for us
        return (1U << 31);
    }
    uint32_t k = 1;
    while (v > k) {
        k <<= 1;
    }
    return k;
}

struct fbp_dl_s * fbp_dl_initialize(
        struct fbp_dl_config_s const * config,
        struct fbp_dl_ll_s const * ll_instance,
        struct fbp_framer_s * framer) {
    FBP_ASSERT(FBP_FRAMER_LINK_SIZE == sizeof(uint64_t)); // assumption for ring_buffer_u64
    FBP_LOGD1("fbp_dl_initialize");
    if (!config || !ll_instance || !framer) {
        FBP_LOGE("invalid arguments");
        return 0;
    }

    uint32_t tx_link_u64_size = config->tx_link_size;
    if (!tx_link_u64_size) {
        tx_link_u64_size = config->rx_window_size;
    }

    uint32_t tx_window_size = to_power_of_two(config->tx_window_size);
    if (tx_window_size < 1) {
        tx_window_size = 1;
    }
    uint32_t rx_window_size = to_power_of_two(config->rx_window_size);

    // Perform single allocation for fbp_dl_s and all buffers.
    size_t offset = 0;
    size_t sz;
    sz = sizeof(struct fbp_dl_s);
    sz = FBP_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t tx_frame_buf_offset = offset;
    sz = sizeof(struct tx_frame_s[2]) / 2 * tx_window_size;
    sz = FBP_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t rx_frame_buf_offset = offset;
    sz = sizeof(struct rx_frame_s[2]) / 2 * rx_window_size;
    sz = FBP_ROUND_UP_TO_MULTIPLE_UNSIGNED(sz, sizeof(uint64_t));
    offset += sz;

    size_t tx_link_buffer_offset = offset;
    FBP_ASSERT(FBP_FRAMER_LINK_SIZE == sizeof(uint64_t));
    FBP_ASSERT((tx_link_buffer_offset & 0x7) == 0);
    sz = tx_link_u64_size * FBP_FRAMER_LINK_SIZE;
    offset += sz;

    struct fbp_dl_s * self = (struct fbp_dl_s *) fbp_alloc_clr(offset);
    uint8_t * mem = (uint8_t *) self;
    self->tx_frame_count = 1;  // negotiate to larger sizes
    self->tx_frame_count_max = tx_window_size;
    self->tx_frames = (struct tx_frame_s *) (mem + tx_frame_buf_offset);
    self->rx_frame_count = rx_window_size;
    self->rx_frames = (struct rx_frame_s *) (mem + rx_frame_buf_offset);
    fbp_rbu64_init(&self->tx_link_buf, (uint64_t *) (mem + tx_link_buffer_offset), tx_link_u64_size);

    self->tx_timeout = config->tx_timeout;
    self->ll_instance = *ll_instance;
    self->ll_size_max = self->ll_instance.send_available(self->ll_instance.user_data);
    FBP_ASSERT(self->ll_size_max >= FBP_FRAMER_MAX_SIZE);
    self->framer = framer;

    self->framer->api.user_data = self;
    self->framer->api.data_fn = on_recv_data;
    self->framer->api.link_fn = on_recv_link;
    self->framer->api.framing_error_fn = on_framing_error;
    reset_state(self);
    send_link(self, FBP_FRAMER_FT_RESET, 0);

    return self;
}

void fbp_dl_register_upper_layer(struct fbp_dl_s * self, struct fbp_dl_api_s const * ul) {
    lock(self);
    self->ul_instance = *ul;
    unlock(self);
}

void fbp_dl_reset_tx_from_event(struct fbp_dl_s * self) {
    if (self->state == ST_DISCONNECTED) {
        FBP_LOGD1("fbp_dl_reset_tx_from_event when already disconnected");
    } else {
        FBP_LOGI("fbp_dl_reset_tx_from_event");
        reset_state(self);
    }
}

int32_t fbp_dl_finalize(struct fbp_dl_s * self) {
    FBP_LOGD1("fbp_dl_finalize");
    if (self) {
        fbp_os_mutex_t mutex = self->mutex;
        lock(self);
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
        }
    }
    return 0;
}

int32_t fbp_dl_status_get(
        struct fbp_dl_s * self,
        struct fbp_dl_status_s * status) {
    if (!status) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    lock(self);
    status->version = FBP_DL_VERSION;
    status->rx = self->rx_status;
    status->rx_framer = self->framer->status;
    status->tx = self->tx_status;
    unlock(self);
    return 0;
}

void fbp_dl_status_clear(struct fbp_dl_s * self) {
    lock(self);
    fbp_memset(&self->rx_status, 0, sizeof(self->rx_status));
    fbp_memset(&self->framer->status, 0, sizeof(self->framer->status));
    fbp_memset(&self->tx_status, 0, sizeof(self->tx_status));
    unlock(self);
}

void fbp_dl_register_mutex(struct fbp_dl_s * self, fbp_os_mutex_t mutex) {
    self->mutex = mutex;
}

void fbp_dl_register_process_request(struct fbp_dl_s * self, fbp_dl_process_request_fn fn, void * user_data) {
    self->process_request_fn = NULL;
    self->process_request_user_data = user_data;
    self->process_request_fn = fn;
}

uint32_t fbp_dl_tx_window_max_get(struct fbp_dl_s * self) {
    return self->tx_frame_count_max;
}

void fbp_dl_tx_window_set(struct fbp_dl_s * self, uint32_t tx_window_size) {
    if (tx_window_size > self->tx_frame_count_max) {
        tx_window_size = self->tx_frame_count_max;
    }
    if (self->tx_frame_count != 1) {
        FBP_LOGE("Duplicate window set - ignore");
        return;
    } else {
        FBP_LOGI("fbp_dl_tx_window_set(%" PRIu32 ")", tx_window_size);
    }
    self->tx_frame_count = tx_window_size;

    uint16_t frame_id = self->tx_frame_last_id;
    uint16_t idx = frame_id & (self->tx_frame_count - 1);
    if (idx) {
        self->tx_frames[idx] = self->tx_frames[0];
        self->tx_frames[0].state = TX_FRAME_ST_IDLE;
    }
}

uint32_t fbp_dl_rx_window_get(struct fbp_dl_s * self) {
    return self->rx_frame_count;
}

int32_t fbp_dl_frame_id_subtract(uint16_t a, uint16_t b) {
    uint16_t c = (a - b) & FBP_FRAMER_FRAME_ID_MAX;
    if (c > (FBP_FRAMER_FRAME_ID_MAX / 2)) {
        return ((int32_t) c) - (FBP_FRAMER_FRAME_ID_MAX + 1);
    } else {
        return (int32_t) c;
    }
}
