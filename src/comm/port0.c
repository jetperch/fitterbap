/*
 * Copyright 2020-2021 Jetperch LLC
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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE
#include "fitterbap/comm/port0.h"
#include "fitterbap/comm/timesync.h"
#include "fitterbap/comm/transport.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/cdef.h"
#include "fitterbap/cstr.h"
#include "fitterbap/log.h"
#include "fitterbap/ec.h"
#include "fitterbap/fsm.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include <inttypes.h>
#include <string.h>


#define TIMESYNC_INTERVAL_MS  (10000)

const char FBP_PORT0_META[] = "{\"type\":\"oam\", \"name\": \"oam\"}";
static const char STATE_TOPIC[] = "0/state";
static const char EVENT_TOPIC[] = "0/ev";
static const char REMOTE_STATUS_TOPIC[] = "0/rstat";
static const char ECHO_ENABLE_META_TOPIC[] = "0/echo/enable";
static const char ECHO_OUTSTANDING_META_TOPIC[] = "0/echo/window";
static const char ECHO_LENGTH_META_TOPIC[] = "0/echo/length";


static const char STATE_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Data link state.\","
    "\"default\": 0,"
    "\"options\": [[0, \"disconnected\"], [1, \"connected\"]],"
    "\"flags\": [\"ro\"],"
    "\"retain\": 1"
    "}";

static const char ECHO_ENABLE_META[] =
    "{"
    "\"dtype\": \"bool\","
    "\"brief\": \"Enable echo\","
    "\"default\": 0,"
    "\"retain\": 1"
    "}";

static const char ECHO_WINDOW_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Number of outstanding echo frames\","
    "\"default\": 8,"
    "\"range\": [1, 64],"  // inclusive
    "\"retain\": 1"
    "}";

static const char ECHO_LENGTH_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Length of each frame in bytes\","
    "\"default\": 256,"
    "\"range\": [8, 256],"  // inclusive
    "\"retain\": 1"
    "}";


enum events_e {
    EV_TX_DISCONNECT,
    EV_TX_CONNECT,
    EV_RX_RESET_REQ,
    EV_TIMEOUT,
    EV_TICK,
    EV_NEGOTIATE_DONE,
    EV_TIMESYNC_DONE,
    EV_META_DONE,
};

enum port0_state_e {
    ST_DISCONNECTED = 0,
    ST_NEGOTIATE,
    ST_TIMESYNC1,
    ST_TIMESYNC2,
    ST_META,
    ST_CONNECTED,
};

struct fbp_port0_s {
    struct fbp_fsm_s fsm;
    enum fbp_port0_mode_e mode;
    struct fbp_dl_s * dl;
    struct fbp_transport_s * transport;
    struct fbp_pubsub_s * pubsub;
    struct fbp_evm_api_s evm;
    char topic_prefix[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    fbp_transport_send_fn send_fn;
    uint8_t meta_port_id;
    uint8_t topic_prefix_length;
    struct fbp_ts_s * timesync;

    int32_t timeout_event_id;
    int32_t tick_event_id;

    uint8_t echo_enable;        ///< Echo on/off control
    uint8_t echo_window;        ///< Number of outstanding echo frames
    uint16_t echo_length;       ///< Echo payload length

    int64_t echo_rx_frame_id;
    int64_t echo_tx_frame_id;
    int64_t echo_buffer[FBP_FRAMER_PAYLOAD_MAX_SIZE / sizeof(int64_t)];
};

#define REQ(op)    ((0x00) | ((FBP_PORT0_OP_##op) & 0x07))
#define RSP(op)    ((0x80) | ((FBP_PORT0_OP_##op) & 0x07))

static const char * event_to_str(struct fbp_fsm_s * self, fbp_fsm_event_t event) {
    (void) self;
    switch (event) {
        case EV_TX_DISCONNECT: return "tx_disconnect";
        case EV_TX_CONNECT: return "tx_connect";
        case EV_RX_RESET_REQ: return "rx_reset_req";
        case EV_TIMEOUT: return "timeout";
        case EV_TICK: return "tick";
        case EV_NEGOTIATE_DONE: return "negotiate_done";
        case EV_TIMESYNC_DONE: return "timesync_done";
        case EV_META_DONE: return "meta_done";

        case FBP_EVENT_ANY: return "any";
        case FBP_EVENT_NULL: return "null";
        case FBP_EVENT_RESET: return "reset";
        case FBP_EVENT_ENTER: return "enter";
        case FBP_EVENT_EXIT: return "exit";
        default: return "event_unknown";
    }
}

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

static inline void emit_event(struct fbp_port0_s * self, fbp_fsm_event_t event) {
    fbp_fsm_event(&self->fsm, event);
}

static void on_timeout(void * user_data, int32_t event_id) {
    struct fbp_port0_s * self = (struct fbp_port0_s *) user_data;
    if (self->timeout_event_id && (self->timeout_event_id != event_id)) {
        FBP_LOGW("timeout event_id mismatch");
    }
    self->timeout_event_id = 0;
    emit_event(self, EV_TIMEOUT);
}

static void timeout_clear(struct fbp_port0_s * self) {
    if (self->timeout_event_id) {
        self->evm.cancel(self->evm.evm, self->timeout_event_id);
        self->timeout_event_id = 0;
    }
}

static void timeout_set(struct fbp_port0_s * self, uint32_t timeout_ms) {
    timeout_clear(self);
    int64_t now = self->evm.timestamp(self->evm.evm);
    int64_t ts = now + FBP_COUNTER_TO_TIME(timeout_ms, 1000);
    self->timeout_event_id = self->evm.schedule(self->evm.evm, ts, on_timeout, self);
}

static void on_tick(void * user_data, int32_t event_id) {
    struct fbp_port0_s * self = (struct fbp_port0_s *) user_data;
    if (self->tick_event_id && (self->tick_event_id != event_id)) {
        FBP_LOGW("tick event_id mismatch");
    }
    self->tick_event_id = 0;
    emit_event(self, EV_TICK);
}

static void tick_clear(struct fbp_port0_s * self) {
    if (self->tick_event_id) {
        self->evm.cancel(self->evm.evm, self->tick_event_id);
        self->tick_event_id = 0;
    }
}

static void tick_set(struct fbp_port0_s * self, uint32_t timeout_ms) {
    tick_clear(self);
    int64_t now = self->evm.timestamp(self->evm.evm);
    int64_t ts = now + FBP_COUNTER_TO_TIME(timeout_ms, 1000);
    self->tick_event_id = self->evm.schedule(self->evm.evm, ts, on_tick, self);
}

static inline void topic_append(struct fbp_port0_s * self, const char * subtopic) {
    fbp_cstr_copy(self->topic_prefix + self->topic_prefix_length, subtopic, FBP_PUBSUB_TOPIC_LENGTH_MAX - self->topic_prefix_length);
}

static inline void topic_reset(struct fbp_port0_s * self) {
    self->topic_prefix[self->topic_prefix_length] = 0;
}

static void echo_send(struct fbp_port0_s * self) {
    while ((self->fsm.state == ST_CONNECTED) && self->echo_enable
            && ((self->echo_tx_frame_id - self->echo_rx_frame_id) < self->echo_window)) {
        self->echo_buffer[0] = self->echo_tx_frame_id++;
        if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(ECHO),
                          (uint8_t *) self->echo_buffer, self->echo_length)) {
            FBP_LOGD1("echo_send error");
        }
    }
}

static uint8_t on_echo_enable(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct fbp_port0_s * self = (struct fbp_port0_s *) user_data;
    if (value->type != FBP_UNION_U32) {
        FBP_LOGW("echo enable, bad type");
        return FBP_ERROR_PARAMETER_INVALID;
    }
    self->echo_enable = value->value.u32 ? 1 : 0;
    if (self->echo_enable) {
        FBP_LOGD1("echo on");
        self->echo_tx_frame_id = 0;
        self->echo_rx_frame_id = 0;
        echo_send(self);
    } else {
        FBP_LOGD1("echo off");
    }
    return 0;
}

static uint8_t on_echo_window(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct fbp_port0_s * self = (struct fbp_port0_s *) user_data;
    if (value->type != FBP_UNION_U32) {
        FBP_LOGW("on_echo_window, bad type");
        return FBP_ERROR_PARAMETER_INVALID;
    }
    uint32_t v = value->value.u32;
    if ((v < 1) || (v > 64)) {
        FBP_LOGW("on_echo_window, bad value");
        return FBP_ERROR_PARAMETER_INVALID;
    }
    FBP_LOGD1("on_echo_window");
    self->echo_window = v;
    echo_send(self);
    return 0;
}

static uint8_t on_echo_length(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct fbp_port0_s * self = (struct fbp_port0_s *) user_data;
    if (value->type != FBP_UNION_U32) {
        FBP_LOGW("on_echo_length, bad type");
        return FBP_ERROR_PARAMETER_INVALID;
    }
    uint32_t v = value->value.u32;
    if ((v < 8) || (v > 256)) {
        FBP_LOGW("on_echo_length, bad value");
        return FBP_ERROR_PARAMETER_INVALID;
    }
    FBP_LOGD1("on_echo_length");
    self->echo_length = v;
    echo_send(self);
    return 0;
}

static void publish(struct fbp_port0_s * self, const char * subtopic, const struct fbp_union_s * value) {
    topic_append(self, subtopic);
    fbp_pubsub_publish(self->pubsub, self->topic_prefix, value, NULL, NULL);
    topic_reset(self);
}

void fbp_port0_on_event_cbk(struct fbp_port0_s * self, enum fbp_dl_event_e event) {
    switch (event) {
        case FBP_DL_EV_RESET_REQUEST:
            emit_event(self, EV_RX_RESET_REQ);
            break;
        case FBP_DL_EV_DISCONNECTED:
            emit_event(self, EV_TX_DISCONNECT);
            break;
        case FBP_DL_EV_CONNECTED:
            emit_event(self, EV_TX_CONNECT);
            break;
        case FBP_DL_EV_TRANSPORT_CONNECTED:
            break;
        case FBP_DL_EV_APP_CONNECTED:
            FBP_LOGN("%s connected", self->topic_prefix);
            break;
        default:
            break;
    }
    publish(self, EVENT_TOPIC, &fbp_union_u32(event));
}

typedef void (*dispatch_fn)(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size);

static void op_status_req(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    (void) msg;
    (void) msg_size;
    struct fbp_dl_status_s status;
    if (self->fsm.state != ST_CONNECTED) {
        FBP_LOGW("status_req, but not connected");
        return;
    }
    int32_t rc = fbp_dl_status_get(self->dl, &status);
    if (!rc) {
        return;
    }
    self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(STATUS),
                  (uint8_t *) &status, sizeof(status));
}

static void op_status_rsp(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    publish(self, REMOTE_STATUS_TOPIC, &fbp_union_bin(msg, msg_size));
}

static void op_echo_req(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    // Send response with same payload
    if (self->fsm.state == ST_CONNECTED) {
        self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(ECHO), msg, msg_size);
    }
}

static void op_echo_rsp(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    if (!self->echo_enable) {
        FBP_LOGD1("echo_rsp but disabled");
        return;
    }
    if (msg_size != self->echo_length) {
        FBP_LOGW("unexpected echo length: %d != %d", (int) msg_size, (int) self->echo_length);
    }
    if (msg_size >= 8) {
        int64_t frame_id;
        memcpy(&frame_id, msg, sizeof(frame_id));
        if (frame_id != self->echo_rx_frame_id) {
            FBP_LOGW("echo frame_id mismatch: %" PRIi64 " != %" PRIi64, frame_id, self->echo_rx_frame_id);
        }
        self->echo_rx_frame_id = frame_id + 1;
    }
    echo_send(self);
}

static int32_t timesync_req_send(struct fbp_port0_s * self) {
    int64_t times[5] = {0, 0, 0, 0, 0};
    times[1] = fbp_time_counter_u64();
    if (self->mode == FBP_PORT0_MODE_SERVER) {
        FBP_LOGW("timesync_req_send by server");
    }
    return self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(TIMESYNC),
                         (uint8_t *) times, sizeof(times));
}

static void op_timesync_req(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    int64_t times[5] = {0, 0, 0, 0, 0};
    if (msg_size < sizeof(times)) {
        return;
    }
    memcpy(times, msg, 2 * sizeof(int64_t));
    times[2] = fbp_time_utc();
    times[3] = times[2];
    if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(TIMESYNC),
                      (uint8_t *) times, sizeof(times))) {
        FBP_LOGW("timestamp reply error");
    } else if (self->mode == FBP_PORT0_MODE_SERVER) {
        emit_event(self, EV_TIMESYNC_DONE);
    }
}

static void op_timesync_rsp(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    int64_t times[5] = {0, 0, 0, 0, 0};
    if (msg_size < sizeof(times)) {
        FBP_LOGW("timesync_rsp too short");
        return;
    }
    memcpy(times, msg, sizeof(times));  // copy to guarantee alignment
    times[4] = fbp_time_counter_u64();
    fbp_ts_update(self->timesync, (uint64_t) times[1], times[2], times[3], (uint64_t) times[4]);

    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        emit_event(self, EV_TIMESYNC_DONE);
    }
}

static int32_t meta_send_next(struct fbp_port0_s * self) {
    uint8_t msg[FBP_FRAMER_PAYLOAD_MAX_SIZE];
    size_t meta_sz;

    if (self->meta_port_id > FBP_TRANSPORT_PORT_MAX) {
        emit_event(self, EV_META_DONE);
        return FBP_ERROR_FULL;
    }

    msg[0] = self->meta_port_id + FBP_PORT0_META_CHAR_OFFSET;
    const char * meta = fbp_transport_meta_get(self->transport, self->meta_port_id);
    if (!meta) {
        meta = "";
    };
    meta_sz = strlen(meta) + 1;
    if (meta_sz > (FBP_FRAMER_PAYLOAD_MAX_SIZE - 1)) {
        FBP_LOGW("truncating meta for port %d", self->meta_port_id);
        meta_sz = FBP_FRAMER_PAYLOAD_MAX_SIZE - 1;  // truncate
    }

    memcpy(msg + 1, meta, meta_sz);
    if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(META), msg, meta_sz + 1)) {
        tick_set(self, 1);
        return FBP_ERROR_BUSY;
    } else {
        ++self->meta_port_id;
        return 0;
    }
}

static void meta_send(struct fbp_port0_s * self) {
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        while (!meta_send_next(self)) {
            // send as many as we can - fill the buffer
        }
    }
}

static void op_meta_rsp(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    char topic[FBP_PUBSUB_TOPIC_LENGTH_MAX] = "port/";
    char * topic_end = topic + FBP_PUBSUB_TOPIC_LENGTH_MAX;
    if (self->fsm.state != ST_META) {
        FBP_LOGW("meta_rsp in state %d", self->fsm.state);
        return;
    }
    if (!msg_size || (msg_size > FBP_FRAMER_PAYLOAD_MAX_SIZE)) {
        FBP_LOGW("empty op_meta_rsp");
        return;
    }
    uint8_t port_id = msg[0] - FBP_PORT0_META_CHAR_OFFSET;
    if (port_id > FBP_TRANSPORT_PORT_MAX) {
        FBP_LOGW("meta_rsp invalid port_id %d", (int) port_id);
        return;
    }
    msg[msg_size - 1] = 0;  // enforce null termination
    if (port_id != self->meta_port_id) {
        FBP_LOGW("meta_rsp unexpected port_id %d != %d", (int) port_id, (int) self->meta_port_id);
    }
    char * t = topic + 5;
    if (port_id >= 10) {
        *t++ = '0' + (port_id / 10);
    }
    *t++ = '0' + (port_id % 10);
    fbp_cstr_copy(t, "/meta", topic_end - t);
    publish(self, topic, &fbp_union_json((char *) &msg[1]));
    self->topic_prefix[self->topic_prefix_length] = 0;
    self->meta_port_id = (port_id < self->meta_port_id) ? self->meta_port_id : (port_id + 1);
    if (self->meta_port_id > FBP_TRANSPORT_PORT_MAX) {
        emit_event(self, EV_META_DONE);
    }
}

static void op_negotiate_req(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    uint32_t req[4];  // version, status, down_window_size, up_window_size
    uint32_t rsp[4] = {FBP_DL_VERSION, 0, 0, 0};
    if (self->mode != FBP_PORT0_MODE_CLIENT) {
        FBP_LOGE("op_negotiate_rsp, but not client on %s", self->topic_prefix);
        rsp[1] = FBP_ERROR_NOT_SUPPORTED;
    } else if (msg_size < sizeof(req)) {
        FBP_LOGE("incompatible negotiate packet on %s", self->topic_prefix);
        rsp[1] = FBP_ERROR_PARAMETER_INVALID;
    } else {
        memcpy(req, msg, sizeof(req));
        if (FBP_DL_VERSION_MAJOR != (req[0] >> 24)) {
            FBP_LOGE("potentially incompatible negotiate version on %s", self->topic_prefix);
            rsp[1] = 0; // could issue warning
        }
        rsp[2] = min_u32(fbp_dl_rx_window_get(self->dl), req[2]);
        rsp[3] = min_u32(fbp_dl_tx_window_max_get(self->dl), req[3]);
        fbp_dl_tx_window_set(self->dl, rsp[3]);
    }
    if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(NEGOTIATE),
                      (uint8_t *) rsp, sizeof(rsp))) {
        FBP_LOGW("negotiate_req send failed on %s", self->topic_prefix);
    }
    emit_event(self, EV_NEGOTIATE_DONE);
}

static void op_negotiate_rsp(struct fbp_port0_s * self, uint8_t *msg, uint32_t msg_size) {
    uint32_t rsp[4];  // version, status, rsv, window_size;
    if (self->mode != FBP_PORT0_MODE_SERVER) {
        FBP_LOGE("op_negotiate_rsp, but not server on %s", self->topic_prefix);
        // fatal error: await timeout
    } else if (msg_size < sizeof(rsp)) {
        FBP_LOGE("incompatible negotiate packeton %s", self->topic_prefix);
        // fatal error: await timeout
    } else {
        memcpy(rsp, msg, sizeof(rsp));
        if (FBP_DL_VERSION_MAJOR != (rsp[0] >> 24)) {
            FBP_LOGE("incompatible negotiate version on %s", self->topic_prefix);
            // fatal error: await timeout.
        } else {
            rsp[2] = min_u32(fbp_dl_tx_window_max_get(self->dl), rsp[2]);
            fbp_dl_tx_window_set(self->dl, rsp[2]);
            emit_event(self, EV_NEGOTIATE_DONE);
        }
    }
}

void fbp_port0_on_recv_cbk(struct fbp_port0_s * self,
                uint8_t port_id,
                enum fbp_transport_seq_e seq,
                uint8_t port_data,
                uint8_t *msg, uint32_t msg_size) {
    if (port_id != 0) {
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        // all messages are single frames only.
        FBP_LOGW("port0 received segmented message");
        return;
    }
    bool req = (port_data & 0x80) == 0;
    uint8_t op = port_data & 0x07;
    dispatch_fn fn = NULL;

    FBP_LOGD2("port0: mode=%d, req=%d, op=%d", (int) self->mode, (int) req, (int) op);

    if (req) {
        switch (op) {
            case FBP_PORT0_OP_STATUS:      fn = op_status_req; break;
            case FBP_PORT0_OP_ECHO:        fn = op_echo_req; break;
            case FBP_PORT0_OP_TIMESYNC:    fn = op_timesync_req; break;
            case FBP_PORT0_OP_NEGOTIATE:   fn = op_negotiate_req; break;
            //case FBP_PORT0_OP_RAW:         fn = op_raw_req; break;
            default:
                break;
        }
    } else {
        switch (op) {
            case FBP_PORT0_OP_STATUS:      fn = op_status_rsp; break;
            case FBP_PORT0_OP_ECHO:        fn = op_echo_rsp; break;
            case FBP_PORT0_OP_TIMESYNC:    fn = op_timesync_rsp; break;
            case FBP_PORT0_OP_META:        fn = op_meta_rsp; break;
            case FBP_PORT0_OP_NEGOTIATE:   fn = op_negotiate_rsp; break;
            //case FBP_PORT0_OP_RAW:         fn = op_raw_rsp; break;
            default:
                break;
        }
    }
    if (fn == NULL) {
        FBP_LOGW("unsupported: mode=%d, req=%d, op=%d", (int) self->mode, (int) req, (int) op);
    } else {
        fn(self, msg, msg_size);
    }
}

static void topic_create(struct fbp_port0_s * self, const char * subtopic, const char * meta,
                         const struct fbp_union_s * value, fbp_pubsub_subscribe_fn src_fn, void * src_user_data) {
    topic_append(self, subtopic);
    fbp_pubsub_meta(self->pubsub, self->topic_prefix, meta);
    if (src_fn) {
        fbp_pubsub_subscribe(self->pubsub, self->topic_prefix,
                             FBP_PUBSUB_SFLAG_RETAIN,
                             src_fn, src_user_data);
    }
    fbp_pubsub_publish(self->pubsub, self->topic_prefix, value, src_fn, src_user_data);
    topic_reset(self);
}

static fbp_fsm_state_t on_enter_disconnected(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    timeout_set(self, 2500);
    tick_clear(self);
    publish(self, STATE_TOPIC, &fbp_union_u32_r(0));
    fbp_dl_reset_tx_from_event(self->dl);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_disconnected_timeout(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    timeout_set(self, 2500);
    fbp_dl_reset_tx_from_event(self->dl);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t negotiate_send_req(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    uint32_t payload[4] = {FBP_DL_VERSION, 0, 0, 0};  // version, status, down_window_size, up_window_size
    payload[2] = fbp_dl_tx_window_max_get(self->dl);
    payload[3] = fbp_dl_rx_window_get(self->dl);
    if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(NEGOTIATE),
                      (uint8_t *) payload, sizeof(payload))) {
        // buffer full, schedule retry
        tick_set(self, 2);
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_negotiate(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    timeout_set(self, 1000);
    if (self->mode == FBP_PORT0_MODE_SERVER) {
        tick_set(self, 50);  // allow reset to stabilize before negotiate_send_req
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_timesync_tick(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    tick_clear(self);
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        if (timesync_req_send(self)) {
            tick_set(self, 2);
        } else {
            tick_set(self, 50);  // retry in case server drops response
        }
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_connected_tick(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    tick_clear(self);
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        if (timesync_req_send(self)) {
            tick_set(self, 1000);
        } else {
            tick_set(self, TIMESYNC_INTERVAL_MS);
        }
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_timesync(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s *self = (struct fbp_port0_s *) fsm;
    tick_clear(self);
    timeout_set(self, 1000);
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        tick_set(self, 2);  // allow queue to empty for fastest response
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_meta(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    timeout_set(self, 1000);
    self->meta_port_id = 0;
    tick_set(self, 1);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_meta_tick(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    meta_send(self);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_connected(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    timeout_clear(self);
    tick_clear(self);
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        tick_set(self, 1);
    }
    publish(self, STATE_TOPIC, &fbp_union_u32_r(1));
    fbp_transport_event_inject(self->transport, FBP_DL_EV_TRANSPORT_CONNECTED);
    echo_send(self);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t only_server(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    if (self->mode == FBP_PORT0_MODE_SERVER) {
        return FBP_STATE_ANY;
    } else {
        return FBP_STATE_SKIP;
    }
}

static fbp_fsm_state_t only_client(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_port0_s * self = (struct fbp_port0_s *) fsm;
    if (self->mode == FBP_PORT0_MODE_CLIENT) {
        return FBP_STATE_ANY;
    } else {
        return FBP_STATE_SKIP;
    }
}

static fbp_fsm_state_t rx_reset_req_guard(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    switch (fsm->state) {
        case ST_DISCONNECTED:  return FBP_STATE_NULL;
        case ST_NEGOTIATE:     return FBP_STATE_NULL;
        default:               return FBP_STATE_ANY;
    }
}

static const struct fbp_fsm_transition_s transitions[] = {  // priority encoded
    {ST_CONNECTED, FBP_STATE_NULL, EV_TICK,             on_connected_tick},
    {ST_CONNECTED, FBP_STATE_NULL, EV_TIMESYNC_DONE,    NULL},
    {ST_DISCONNECTED, ST_NEGOTIATE, EV_TX_CONNECT,      NULL},

    {ST_NEGOTIATE, ST_TIMESYNC1, EV_NEGOTIATE_DONE,     only_client},
    {ST_NEGOTIATE, ST_META, EV_NEGOTIATE_DONE,          only_server},
    {ST_NEGOTIATE, FBP_STATE_NULL, EV_TICK,             negotiate_send_req},
    {ST_META, FBP_STATE_NULL, EV_TICK,                  on_meta_tick},
    {ST_META, ST_CONNECTED, EV_META_DONE,               NULL},

    {ST_TIMESYNC1, FBP_STATE_NULL, EV_TICK,             on_timesync_tick},
    {ST_TIMESYNC1, ST_TIMESYNC2, EV_TIMESYNC_DONE,      NULL},
    {ST_TIMESYNC2, FBP_STATE_NULL, EV_TICK,             on_timesync_tick},
    {ST_TIMESYNC2, ST_META, EV_TIMESYNC_DONE,           NULL},

    {ST_DISCONNECTED, FBP_STATE_NULL, EV_TIMEOUT,       on_disconnected_timeout},

    {FBP_STATE_ANY, ST_DISCONNECTED, EV_RX_RESET_REQ,   rx_reset_req_guard},
    {FBP_STATE_ANY, ST_DISCONNECTED, EV_TX_DISCONNECT,  NULL},
    {FBP_STATE_ANY, ST_DISCONNECTED, EV_TIMEOUT,        NULL},
    {FBP_STATE_ANY, ST_DISCONNECTED, FBP_EVENT_RESET,   NULL},
};

static const struct fbp_fsm_state_s states[] = {
    {ST_DISCONNECTED,   "disconnected", on_enter_disconnected, NULL},
    {ST_NEGOTIATE,      "negotiate",    on_enter_negotiate, NULL},
    {ST_TIMESYNC1,      "timesync1",    on_enter_timesync, NULL},
    {ST_TIMESYNC2,      "timesync2",    on_enter_timesync, NULL},
    {ST_META,           "meta",         on_enter_meta, NULL},
    {ST_CONNECTED,      "connected",    on_enter_connected, NULL},
};

struct fbp_port0_s * fbp_port0_initialize(enum fbp_port0_mode_e mode,
        struct fbp_dl_s * dl,
        struct fbp_evm_api_s * evm,
        struct fbp_transport_s * transport, fbp_transport_send_fn send_fn,
        struct fbp_pubsub_s * pubsub, const char * topic_prefix,
        struct fbp_ts_s * timesync) {
    struct fbp_port0_s * p = fbp_alloc_clr(sizeof(struct fbp_port0_s));
    FBP_ASSERT_ALLOC(p);

    p->fsm.name = (mode == FBP_PORT0_MODE_CLIENT) ? "port0_client" : "port0_server";
    p->fsm.state = ST_DISCONNECTED;
    p->fsm.states = states;
    p->fsm.states_count = FBP_ARRAY_SIZE(states);
    p->fsm.transitions = transitions;
    p->fsm.transitions_count = FBP_ARRAY_SIZE(transitions);
    p->fsm.event_name_fn = event_to_str;
    p->fsm.reentrant = 0;

    p->mode = mode;
    p->dl = dl;
    p->evm = *evm;
    p->transport = transport;
    p->pubsub = pubsub;
    fbp_cstr_copy(p->topic_prefix, topic_prefix, sizeof(p->topic_prefix));
    p->topic_prefix_length = (uint8_t) strlen(p->topic_prefix);
    p->send_fn = send_fn;
    p->echo_enable = 0;
    p->echo_window = 8;
    p->echo_length = FBP_FRAMER_PAYLOAD_MAX_SIZE;
    p->timesync = timesync;

    topic_create(p, STATE_TOPIC, STATE_META, &fbp_union_u32_r(0), NULL, NULL);
    topic_create(p, ECHO_ENABLE_META_TOPIC, ECHO_ENABLE_META, &fbp_union_u32_r(p->echo_enable), on_echo_enable, p);
    topic_create(p, ECHO_OUTSTANDING_META_TOPIC, ECHO_WINDOW_META, &fbp_union_u32_r(p->echo_window), on_echo_window, p);
    topic_create(p, ECHO_LENGTH_META_TOPIC, ECHO_LENGTH_META, &fbp_union_u32_r(p->echo_length), on_echo_length, p);

    fbp_fsm_reset(&p->fsm);
    return p;
}

void fbp_port0_finalize(struct fbp_port0_s * self) {
    if (self) {
        fbp_free(self);
    }
}
