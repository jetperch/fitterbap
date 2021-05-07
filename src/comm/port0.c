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

#include "fitterbap/comm/port0.h"
#include "fitterbap/comm/transport.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/cstr.h"
#include "fitterbap/log.h"
#include "fitterbap/ec.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include <inttypes.h>
#include <string.h>


const char FBP_PORT0_META[] = "{\"type\":\"oam\", \"name\": \"oam\"}";
static const char EV_TOPIC[] = "port/0/ev";
static const char TX_TOPIC[] = "port/0/tx";
static const char REMOTE_STATUS_TOPIC[] = "port/0/rstat";
static const char ECHO_ENABLE_META_TOPIC[] = "port/0/echo/enable";
static const char ECHO_OUTSTANDING_META_TOPIC[] = "port/0/echo/window";
static const char ECHO_LENGTH_META_TOPIC[] = "port/0/echo/length";

#define META_OUTSTANDING_MAX (4)


static const char TX_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Data link TX state.\","
    "\"default\": 0,"
    "\"options\": [[0, \"disconnected\"], [1, \"connected\"]],"
    "\"flags\": [\"ro\"],"
    "\"retain\": 1"
    "}";

static const char EV_META[] =
    "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Data link event.\","
    "\"default\": 0,"
    "\"options\": [[0, \"unknown\"], [1, \"rx_reset\"], [2, \"tx_disconnected\"], [3, \"tx_connected\"]],"
    "\"flags\": [\"ro\"]"
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

enum port0_state_e {
    ST_INIT = 0,
    ST_META,
    ST_DISCONNECTED,
    ST_CONNECTED,
};

struct fbp_port0_s {
    enum fbp_port0_mode_e mode;
    struct fbp_dl_s * dl;
    struct fbp_transport_s * transport;
    struct fbp_pubsub_s * pubsub;
    char topic_prefix[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    fbp_transport_send_fn send_fn;
    uint8_t meta_tx_port_id;
    uint8_t meta_rx_port_id;
    uint8_t state;
    uint8_t topic_prefix_length;

    uint8_t echo_enable;        ///< Echo on/off control
    uint8_t echo_window;        ///< Number of outstanding echo frames
    uint16_t echo_length;       ///< Echo payload length

    int64_t echo_rx_frame_id;
    int64_t echo_tx_frame_id;
    int64_t echo_buffer[FBP_FRAMER_PAYLOAD_MAX_SIZE / sizeof(int64_t)];
};

#define pack_req(op, cmd_meta) \
     ((FBP_PORT0_OP_##op & 0x07) | (0x00) | (((uint16_t) cmd_meta) << 8))

#define pack_rsp(op, cmd_meta) \
     ((FBP_PORT0_OP_##op & 0x07) | (0x08) | (((uint16_t) cmd_meta) << 8))

static inline void topic_append(struct fbp_port0_s * self, const char * subtopic) {
    fbp_cstr_copy(self->topic_prefix + self->topic_prefix_length, subtopic, FBP_PUBSUB_TOPIC_LENGTH_MAX - self->topic_prefix_length);
}

static inline void topic_reset(struct fbp_port0_s * self) {
    self->topic_prefix[self->topic_prefix_length] = 0;
}

static void echo_send(struct fbp_port0_s * self) {
    while ((self->state == ST_CONNECTED) && self->echo_enable
            && ((self->echo_tx_frame_id - self->echo_rx_frame_id) < self->echo_window)) {
        self->echo_buffer[0] = self->echo_tx_frame_id++;
        uint16_t port_data = pack_req(ECHO, 0);
        if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, port_data,
                          (uint8_t *) self->echo_buffer, self->echo_length, FBP_PORT0_TIMEOUT_MS)) {
            FBP_LOGW("echo_send error");
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
        FBP_LOGW("echo on");
        self->echo_tx_frame_id = 0;
        self->echo_rx_frame_id = 0;
        echo_send(self);
    } else {
        FBP_LOGW("echo off");
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
    FBP_LOGI("on_echo_window");
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
    FBP_LOGI("on_echo_length");
    self->echo_length = v;
    echo_send(self);
    return 0;
}

static void publish(struct fbp_port0_s * self, const char * subtopic, const struct fbp_union_s * value) {
    topic_append(self, subtopic);
    fbp_pubsub_publish(self->pubsub, self->topic_prefix, value, NULL, NULL);
    topic_reset(self);
}

static void meta_scan(struct fbp_port0_s * self) {
    uint8_t payload = 0;
    while ((self->meta_tx_port_id <= FBP_TRANSPORT_PORT_MAX) && ((self->meta_tx_port_id - self->meta_rx_port_id) < META_OUTSTANDING_MAX)) {
        uint16_t port_data = pack_req(META, self->meta_tx_port_id);
        if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, port_data, &payload, 1, FBP_PORT0_TIMEOUT_MS)) {
            FBP_LOGW("meta_scan send error");
        }
        ++self->meta_tx_port_id;
    }
}

void fbp_port0_on_event_cbk(struct fbp_port0_s * self, enum fbp_dl_event_e event) {
    if ((FBP_DL_EV_RX_RESET_REQUEST == event) && (self->mode == FBP_PORT0_MODE_CLIENT)) {
        FBP_LOGI("port0 rx reset -> tx reset");
        fbp_dl_reset_tx_from_event(self->dl);
    }
    publish(self, EV_TOPIC, &fbp_union_u32(event));

    switch (event) {
        case FBP_DL_EV_RX_RESET_REQUEST:
            break;
        case FBP_DL_EV_TX_DISCONNECTED:
            if (self->state == ST_CONNECTED) {
                self->state = ST_DISCONNECTED;
            } else if (self->state == ST_META) {
                self->meta_rx_port_id = 0;
                self->meta_tx_port_id = 0;
            }
            publish(self, TX_TOPIC, &fbp_union_u32_r(0));
            break;
        case FBP_DL_EV_TX_CONNECTED:
            if (self->state == ST_INIT) {
                self->state = ST_META;
                if (self->mode == FBP_PORT0_MODE_SERVER) {
                    FBP_LOGI("port0 meta_scan");
                    meta_scan(self);
                }
            } else {
                self->state = ST_CONNECTED;
                publish(self, TX_TOPIC, &fbp_union_u32_r(1));
            }
            break;
        default:
            break;
    }
}


typedef void (*dispatch_fn)(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size);

static void op_status_req(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    (void) msg;
    (void) msg_size;
    struct fbp_dl_status_s status;
    int32_t rc = fbp_dl_status_get(self->dl, &status);
    if (!rc) {
        return;
    }
    self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, pack_rsp(STATUS, 0),
                  (uint8_t *) &status, sizeof(status), FBP_PORT0_TIMEOUT_MS);
}

static void op_status_rsp(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    publish(self, REMOTE_STATUS_TOPIC, &fbp_union_bin(msg, msg_size));
}

static void op_echo_req(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    // Send response with same payload
    self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, pack_rsp(ECHO, cmd_meta), msg, msg_size, FBP_PORT0_TIMEOUT_MS);
}

static void op_echo_rsp(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) cmd_meta;
    if (!self->echo_enable) {
        FBP_LOGI("echo_rsp but disabled");
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

static void op_timesync_req(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    int64_t times[4];
    if (msg_size < 8) {
        return;
    }
    memcpy(&times[0], msg, sizeof(times[0]));
    times[1] = fbp_time_utc();
    times[2] = times[1];
    times[3] = 0;
    uint16_t port_data = pack_rsp(TIMESYNC, cmd_meta);
    if (self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, port_data,
                      (uint8_t *) times, sizeof(times), FBP_PORT0_TIMEOUT_MS)) {
        FBP_LOGW("timestamp reply error");
    }
}

static void op_meta_req(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) msg;  // ignore
    (void) msg_size;
    uint16_t port_data = pack_rsp(META, cmd_meta);
    size_t meta_sz  = 1;
    const char * meta = fbp_transport_meta_get(self->transport, cmd_meta);
    if (!meta) {
        meta = "";
    } else {
        meta_sz = strlen(meta) + 1;
    }
    if (meta_sz > FBP_FRAMER_PAYLOAD_MAX_SIZE) {
        FBP_LOGW("on_meta_req too big");
        return;
    }
    if ((cmd_meta <= FBP_TRANSPORT_PORT_MAX) && meta) {
        self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, port_data,
                      (uint8_t *) meta, (uint32_t) meta_sz, FBP_PORT0_TIMEOUT_MS);
    } else {
        uint8_t empty = 0;
        self->send_fn(self->transport, 0, FBP_TRANSPORT_SEQ_SINGLE, port_data, &empty, 1, FBP_PORT0_TIMEOUT_MS);
    }
}

static void op_meta_rsp(struct fbp_port0_s * self, uint8_t cmd_meta, uint8_t *msg, uint32_t msg_size) {
    (void) msg_size;
    char topic[FBP_PUBSUB_TOPIC_LENGTH_MAX] = "port/";
    char * topic_end = topic + FBP_PUBSUB_TOPIC_LENGTH_MAX;
    uint8_t port_id = cmd_meta;
    if (port_id != self->meta_rx_port_id) {
        FBP_LOGW("meta_rsp unexpected port_id %d != %d", (int) port_id, (int) self->meta_rx_port_id);
    }
    char * t = topic + 5;
    if (port_id >= 10) {
        *t++ = '0' + (port_id / 10);
    }
    *t++ = '0' + (port_id % 10);
    fbp_cstr_copy(t, "/meta", topic_end - t);
    publish(self, topic, &fbp_union_json((char *) msg));
    self->topic_prefix[self->topic_prefix_length] = 0;

    self->meta_rx_port_id = (port_id >= self->meta_tx_port_id) ? self->meta_tx_port_id : (port_id + 1);
    if (self->meta_rx_port_id > FBP_TRANSPORT_PORT_MAX) {
        if (self->state == ST_META) {
            self->state = ST_CONNECTED;
            FBP_LOGI("port0: scan completed, connected");
            publish(self, TX_TOPIC, &fbp_union_u32_r(1));
        }
    } else if (self->mode == FBP_PORT0_MODE_SERVER) {
        meta_scan(self);
    }
}

void fbp_port0_on_recv_cbk(struct fbp_port0_s * self,
                uint8_t port_id,
                enum fbp_transport_seq_e seq,
                uint16_t port_data,
                uint8_t *msg, uint32_t msg_size) {
    if (port_id != 0) {
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        // all messages are single frames only.
        FBP_LOGW("port0 received segmented message");
        return;
    }
    uint8_t cmd_meta = (port_data >> 8) & 0xff;
    bool req = (port_data & 0x08) == 0;
    uint8_t op = port_data & 0x07;
    dispatch_fn fn = NULL;


    if (req) {
        switch (op) {
            case FBP_PORT0_OP_STATUS:      fn = op_status_req; break;
            case FBP_PORT0_OP_ECHO:        fn = op_echo_req; break;
            case FBP_PORT0_OP_TIMESYNC:    fn = op_timesync_req; break;
            case FBP_PORT0_OP_META:        fn = op_meta_req; break;
            //case FBP_PORT0_OP_RAW:         fn = op_raw_req; break;
            default:
                break;
        }
    } else {
        switch (op) {
            case FBP_PORT0_OP_STATUS:      fn = op_status_rsp; break;
            case FBP_PORT0_OP_ECHO:        fn = op_echo_rsp; break;
            //case FBP_PORT0_OP_TIMESYNC:    fn = op_timesync_rsp; break;
            case FBP_PORT0_OP_META:        fn = op_meta_rsp; break;
            //case FBP_PORT0_OP_RAW:         fn = op_raw_rsp; break;
            default:
                break;
        }
    }
    if (fn == NULL) {
        FBP_LOGW("unsupported: mode=%d, req=%d, op=%d", (int) self->mode, (int) req, (int) op);
    } else {
        fn(self, cmd_meta, msg, msg_size);
    }
}

static void pubsub_create(struct fbp_port0_s * self, const char * subtopic, const char * meta,
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

struct fbp_port0_s * fbp_port0_initialize(enum fbp_port0_mode_e mode,
        struct fbp_dl_s * dl,
        struct fbp_transport_s * transport, fbp_transport_send_fn send_fn,
        struct fbp_pubsub_s * pubsub, const char * topic_prefix) {
    struct fbp_port0_s * p = fbp_alloc_clr(sizeof(struct fbp_port0_s));
    FBP_ASSERT_ALLOC(p);
    p->mode = mode;
    p->dl = dl;
    p->transport = transport;
    p->pubsub = pubsub;
    fbp_cstr_copy(p->topic_prefix, topic_prefix, sizeof(p->topic_prefix));
    p->topic_prefix_length = (uint8_t) strlen(p->topic_prefix);
    p->send_fn = send_fn;
    p->echo_enable = 0;
    p->echo_window = 8;
    p->echo_length = 256;

    pubsub_create(p, EV_TOPIC, EV_META, &fbp_union_u32_r(0), NULL, NULL);
    pubsub_create(p, TX_TOPIC, TX_META, &fbp_union_u32_r(0), NULL, NULL);
    pubsub_create(p, ECHO_ENABLE_META_TOPIC, ECHO_ENABLE_META, &fbp_union_u32_r(p->echo_enable), on_echo_enable, p);
    pubsub_create(p, ECHO_OUTSTANDING_META_TOPIC, ECHO_WINDOW_META, &fbp_union_u32_r(p->echo_window), on_echo_window, p);
    pubsub_create(p, ECHO_LENGTH_META_TOPIC, ECHO_LENGTH_META, &fbp_union_u32_r(p->echo_length), on_echo_length, p);

    return p;
}

void fbp_port0_finalize(struct fbp_port0_s * self) {
    if (self) {
        fbp_free(self);
    }
}
