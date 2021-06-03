/*
 * Copyright 2021 Jetperch LLC
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

// #define FBP_LOG_LEVEL FBP_LOG_LEVEL_INFO
#include "fitterbap/comm/pubsub_port.h"
#include "fitterbap/memory/bbuf.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/fsm.h"
#include "fitterbap/log.h"
#include "fitterbap/cstr.h"
#include "fitterbap/os/task.h"


#define FEEDBACK_TOPIC_PREFIX "_/fb/"
FBP_STATIC_ASSERT((int) FBP_UNION_FLAG_RETAIN == (int) FBP_PUBSUB_SFLAG_RETAIN, retain_flags_must_be_same);

enum state_e {
    ST_DISCONNECTED,   // Not connected
    ST_NEGOTIATE_REQ,  // Send/receive negotiate request
    ST_NEGOTIATE_RSP,  // Send/receive negotiate response
    ST_TOPIC_LIST,     // Send/receive topic list
    ST_UPDATE_SEND,    // Send retained values
    ST_UPDATE_RECV,    // Receive retained values
    ST_CONN_REQ_SEND,
    ST_CONN_RSP_SEND,
    ST_CONN_RSP_RECV,
    ST_CONNECTED,        // Normal operation
};

enum events_e {
    EV_DISCONNECT,
    EV_TRANSPORT_CONNECT,
    EV_SENT,
    EV_RECV,
    EV_RECV_NEGOTIATE,
    EV_END_TOPIC,
    EV_TICK,
    EV_TIMEOUT,
};

struct fbp_pubsubp_s {
    struct fbp_fsm_s fsm;
    uint64_t client_connection_count;
    uint64_t server_connection_count;
    uint8_t source;  // 0=server, 1=client
    uint8_t port_id;
    uint8_t mode;
    int32_t timeout_event_id;
    int32_t tick_event_id;
    struct fbp_pubsub_s * pubsub;
    struct fbp_transport_s * transport;
    struct fbp_evm_api_s evm;

    uint8_t msg[FBP_FRAMER_PAYLOAD_MAX_SIZE];
    char feedback_topic[FBP_PUBSUB_TOPIC_LENGTH_MAX];
};

const char FBP_PUBSUBP_META[] = "{\"type\":\"pubsub\", \"name\":\"pubsub\"}";

static const char * event_to_str(struct fbp_fsm_s * self, fbp_fsm_event_t event) {
    (void) self;
    switch (event) {
        case EV_DISCONNECT: return "disconnect";
        case EV_TRANSPORT_CONNECT: return "transport_connect";
        case EV_SENT: return "sent";
        case EV_RECV: return "recv";
        case EV_RECV_NEGOTIATE: return "recv_negotiate";
        case EV_END_TOPIC: return "end_topic";
        case EV_TICK: return "tick";
        case EV_TIMEOUT: return "timeout";

        case FBP_EVENT_ANY: return "any";
        case FBP_EVENT_NULL: return "null";
        case FBP_EVENT_RESET: return "reset";
        case FBP_EVENT_ENTER: return "enter";
        case FBP_EVENT_EXIT: return "exit";
        default: return "event_unknown";
    }
}

static inline void emit_event(struct fbp_pubsubp_s * self, fbp_fsm_event_t event) {
    fbp_fsm_event(&self->fsm, event);
}

static inline bool is_client(struct fbp_pubsubp_s *self) {
    return (self->mode == FBP_PUBSUBP_MODE_UPSTREAM);
}

static void on_timeout(void * user_data, int32_t event_id) {
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) user_data;
    if (self->timeout_event_id && (self->timeout_event_id != event_id)) {
        FBP_LOGW("timeout event_id mismatch");
    }
    self->timeout_event_id = 0;
    emit_event(self, EV_TIMEOUT);
}

static void timeout_clear(struct fbp_pubsubp_s * self) {
    if (self->timeout_event_id) {
        self->evm.cancel(self->evm.evm, self->timeout_event_id);
        self->timeout_event_id = 0;
    }
}

static void timeout_set(struct fbp_pubsubp_s * self, uint32_t timeout_ms) {
    timeout_clear(self);
    int64_t now = self->evm.timestamp(self->evm.evm);
    int64_t ts = now + FBP_COUNTER_TO_TIME(timeout_ms, 1000);
    self->timeout_event_id = self->evm.schedule(self->evm.evm, ts, on_timeout, self);
}

static void on_tick(void * user_data, int32_t event_id) {
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) user_data;
    if (self->tick_event_id && (self->tick_event_id != event_id)) {
        FBP_LOGW("tick event_id mismatch");
    }
    self->tick_event_id = 0;
    emit_event(self, EV_TICK);
}

static void tick_clear(struct fbp_pubsubp_s * self) {
    if (self->tick_event_id) {
        self->evm.cancel(self->evm.evm, self->tick_event_id);
        self->tick_event_id = 0;
    }
}

static void tick_set(struct fbp_pubsubp_s * self, uint32_t timeout_ms) {
    tick_clear(self);
    int64_t now = self->evm.timestamp(self->evm.evm);
    int64_t ts = now + FBP_COUNTER_TO_TIME(timeout_ms, 1000);
    self->tick_event_id = self->evm.schedule(self->evm.evm, ts, on_tick, self);
}

static void construct_feedback_topic(struct fbp_pubsubp_s * self) {
    const char * feedback_prefix = FEEDBACK_TOPIC_PREFIX;
    char * feedback_topic = self->feedback_topic;
    while (*feedback_prefix) {
        *feedback_topic++ = *feedback_prefix++;
    }
    uint32_t feedback_value = (intptr_t) self;
    for (int i = 0; i < 7; ++i) {  // skip least significant nibble.
        *feedback_topic++ = fbp_cstr_u4_to_hex(feedback_value >> (4 * (7 - i)));
    }
    *feedback_topic = 0;
}

static void publish_feedback_topic(struct fbp_pubsubp_s * self) {
    fbp_pubsub_publish(self->pubsub, self->feedback_topic, &fbp_union_u32(1), 0, 0);
}

int32_t fbp_pubsubp_transport_register(struct fbp_pubsubp_s * self,
                                        uint8_t port_id,
                                        struct fbp_transport_s * transport) {
    self->port_id = port_id;
    self->transport = transport;
    int32_t rc = fbp_transport_port_register(
            transport,
            port_id,
            FBP_PUBSUBP_META,
            (fbp_transport_event_fn) fbp_pubsubp_on_event,
            (fbp_transport_recv_fn) fbp_pubsubp_on_recv,
            self);
    if (rc) {
        return rc;
    }
    return 0;
}

static int32_t send_negotiate_req(struct fbp_pubsubp_s *self) {
    struct fbp_pubsubp_msg_negotiate_s msg = {
            .version = FBP_DL_VERSION,
            .status = 0,
            .resolution = 0,
            .msg_type = 0,
            .rsv1_u8 = 0,
            .client_connection_count = 0,
            .server_connection_count = self->server_connection_count,
    };
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              FBP_PUBSUBP_MSG_NEGOTIATE, (uint8_t *) &msg, sizeof(msg), 0);
}

static int32_t send_negotiate_rsp(struct fbp_pubsubp_s *self) {
    struct fbp_pubsubp_msg_negotiate_s msg = {
            .version = FBP_DL_VERSION,
            .status = 0,
            .resolution = self->source,
            .msg_type = 1,
            .rsv1_u8 = 0,
            .client_connection_count = self->client_connection_count,
            .server_connection_count = self->server_connection_count,
    };
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              FBP_PUBSUBP_MSG_NEGOTIATE, (uint8_t *) &msg, sizeof(msg), 0);
}

static int32_t send_conn_req(struct fbp_pubsubp_s *self) {
    uint8_t payload[2] = {0, 0};
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              FBP_PUBSUBP_MSG_CONNECTED, payload, sizeof(payload), 0);
}

static int32_t send_conn_rsp(struct fbp_pubsubp_s *self) {
    uint8_t payload[2] = {0, 1};
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              FBP_PUBSUBP_MSG_CONNECTED, payload, sizeof(payload), 0);
}


void fbp_pubsubp_on_event(struct fbp_pubsubp_s *self, enum fbp_dl_event_e event) {
    switch (event) {
        case FBP_DL_EV_RESET_REQUEST:  // intentional fall-though
        case FBP_DL_EV_DISCONNECTED:
            emit_event(self, EV_DISCONNECT);
            break;
        case FBP_DL_EV_TRANSPORT_CONNECTED:
            emit_event(self, EV_TRANSPORT_CONNECT);
            break;
        default:
            break;
    }
}

#define decode(var_, value_) \
    if (payload_len != sizeof(var_)) { \
        FBP_LOGW("invalid payload"); \
        return;                \
    } else { \
        var_ = (value_); \
    }

static void on_negotiate(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                         uint8_t port_data,
                         uint8_t *msg, uint32_t msg_size) {
    (void) port_data;  // unused by msg_conn
    struct fbp_pubsubp_msg_negotiate_s negotiate;
    if (msg_size < sizeof(negotiate)) {
        FBP_LOGW("invalid negotiate size: %d", (int) msg_size);
        return;
    } else if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    memcpy(&negotiate, msg, sizeof(negotiate));
    if ((negotiate.version >> 24) != FBP_DL_VERSION_MAJOR) {
        FBP_LOGW("version mismatch - caution!");
    }

    if (is_client(self)) {
        if ((negotiate.msg_type != 0) || (self->fsm.state != ST_NEGOTIATE_REQ)) {
            FBP_LOGW("client received unexpected negotiate %d", (int) negotiate.msg_type);
            // will force into ST_NEGOTIATE_RSP
        }
        self->source = 0;
        self->server_connection_count = negotiate.server_connection_count;
        if ((self->server_connection_count <= 1) || (self->client_connection_count > self->server_connection_count)) {
            self->source = 1;  // client provides state
        }
    } else {
        if ((negotiate.msg_type != 1) || (self->fsm.state != ST_NEGOTIATE_RSP)) {
            FBP_LOGW("server received unexpected negotiate %d", (int) negotiate.msg_type);
            return;
        } else if (negotiate.status != 0) {
            FBP_LOGW("negotiate status error: %d", (int) negotiate.status);
            return;
        }
        self->client_connection_count = negotiate.client_connection_count;
        self->source = negotiate.resolution;
    }
    FBP_LOGI("server=%d, client=%d, resolution=%d=%s",
             (int) self->server_connection_count, (int) self->client_connection_count,
             (int) self->source,
             self->source ? "client" : "server");

    emit_event(self, EV_RECV_NEGOTIATE);
}

static uint8_t port_data_to_flags(uint8_t port_data) {
    uint8_t flags = 0;
    if (port_data & FBP_PUBSUBP_PORT_DATA_RETAIN_BIT) {
        flags |= FBP_PUBSUB_SFLAG_RETAIN;
    }
    return flags;
}

static void on_topic_list(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                 uint8_t port_data,
                                 uint8_t *msg, uint32_t msg_size) {
    if (is_client(self) || (self->fsm.state != ST_TOPIC_LIST)) {
        FBP_LOGW("unexpected topic list");
        return;
    } else if ((seq != FBP_TRANSPORT_SEQ_SINGLE)  || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_list");
        return;
    }
    FBP_LOGI("topic list");
    fbp_pubsub_subscribe(self->pubsub, "", FBP_PUBSUB_SFLAG_NOPUB | FBP_PUBSUB_SFLAG_REQ,
                         (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
    uint8_t flags = port_data_to_flags(port_data);
    char * topic = (char *) msg;
    char * end = topic;
    char end_ch;
    while (*topic) {
        while (*end && *end != FBP_PUBSUB_UNIT_SEP_CHR) {
            ++end;
        }
        end_ch = *end;
        *end = 0;
        FBP_LOGI("topic list subscribe: %s flags=0x%02x", topic, (unsigned int) flags);
        fbp_pubsub_subscribe(self->pubsub, topic, flags,
                             (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
        if (end_ch) {
            topic = end + 1;
        } else {
            topic = end;
        }
        end = topic;
    }
    emit_event(self, EV_RECV);
}

static void on_topic_add(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                uint8_t port_data,
                                uint8_t *msg, uint32_t msg_size) {
    if (is_client(self) || (self->fsm.state != ST_CONNECTED)) {
        FBP_LOGW("unexpected topic_add");
        return;
    } else if ((seq != FBP_TRANSPORT_SEQ_SINGLE) || (msg_size > FBP_PUBSUB_TOPIC_LENGTH_MAX) || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_add");
        return;
    }
    FBP_LOGI("topic add %s", msg);
    uint8_t flags = port_data_to_flags(port_data);
    fbp_pubsub_subscribe(self->pubsub, (char *) msg, flags,
                           (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static void on_topic_remove(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                   uint8_t port_data,
                                   uint8_t *msg, uint32_t msg_size) {
    (void) port_data;
    if (is_client(self) || (self->fsm.state != ST_CONNECTED)) {
        FBP_LOGW("unexpected topic_remove");
        return;
    } else if ((seq != FBP_TRANSPORT_SEQ_SINGLE) || (msg_size > FBP_PUBSUB_TOPIC_LENGTH_MAX) || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_remove");
        return;
    }
    FBP_LOGI("topic remove %s", msg);
    fbp_pubsub_unsubscribe(self->pubsub, (char *) msg,
                           (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static void on_publish(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                       uint8_t port_data,
                       uint8_t *msg, uint32_t msg_size) {
    if ((self->fsm.state != ST_UPDATE_RECV) && (self->fsm.state != ST_CONNECTED)) {
        FBP_LOGW("unexpected publish");
        return;
    } else if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    uint8_t flags = port_data_to_flags(port_data);
    uint8_t msg_type = port_data & FBP_PUBSUBP_PORT_DATA_MSG_MASK;
    if (msg_type != FBP_PUBSUBP_MSG_PUBLISH) {
        FBP_LOGW("invalid port_data: %d", (int) port_data);
        return;
    }
    if (msg_size < 5) {  // type, rsv, topic len, topic null terminator, payload length
        FBP_LOGW("msg too small");
        return;
    }
    uint8_t type = msg[0];
    if (msg[2] > FBP_PUBSUB_TOPIC_LENGTH_MAX) {
        FBP_LOGW("topic too long");
        return;
    }
    uint8_t topic_len = msg[2];  // 32 max
    char * topic = (char *) (msg + 3);  // type, rsv, topic len byte + topic bytes
    uint32_t sz = 3 + topic_len;  // type, rsv, topic length byte + topic bytes
    if (msg_size < (sz + 1)) {  // +1 payload length
        FBP_LOGW("msg too small: %d < %d", (int) msg_size, (int) sz);
        return;
    }
    if (topic[topic_len - 1]) {
        FBP_LOGW("topic invalid: missing null terminator");
        return;
    }

    uint8_t payload_len = msg[sz++];
    uint8_t *payload = &msg[sz];
    sz += payload_len;
    if (msg_size < sz) {
        FBP_LOGW("msg too small: %d < %d", (int) msg_size, (int) sz);
        return;
    } else if (msg_size > FBP_FRAMER_PAYLOAD_MAX_SIZE) {
        FBP_LOGW("msg too big: %d > %d", (int) msg_size, (int) FBP_FRAMER_PAYLOAD_MAX_SIZE);
        return;
    }

    // parse message
    struct fbp_union_s value = fbp_union_null();
    value.type = type;
    value.flags = flags;
    switch (type) {
        case FBP_UNION_NULL:
            break;
        case FBP_UNION_STR:
        case FBP_UNION_JSON: // intentional fall-through
            if (payload[payload_len - 1]) {
                FBP_LOGW("invalid payload string");
                return;
            } else {
                value.value.str = (char *) payload;
                value.flags = 0;  // no flags supported
                value.size = payload_len;
            }
            break;
        case FBP_UNION_BIN:
            value.value.bin = payload;
            value.flags = 0;  // no flags supported
            value.size = payload_len;
            break;
        case FBP_UNION_F32: decode(value.value.u32, FBP_BBUF_DECODE_U32_LE(payload)); break;
        case FBP_UNION_F64: decode(value.value.u64, FBP_BBUF_DECODE_U64_LE(payload)); break;
        case FBP_UNION_U8: decode(value.value.u8, payload[0]); break;
        case FBP_UNION_U16: decode(value.value.u16, FBP_BBUF_DECODE_U16_LE(payload)); break;
        case FBP_UNION_U32: decode(value.value.u32, FBP_BBUF_DECODE_U32_LE(payload)); break;
        case FBP_UNION_U64: decode(value.value.u64, FBP_BBUF_DECODE_U64_LE(payload)); break;
        case FBP_UNION_I8: decode(value.value.i8, (int8_t) payload[0]); break;
        case FBP_UNION_I16: decode(value.value.i16, (int16_t) FBP_BBUF_DECODE_U16_LE(payload)); break;
        case FBP_UNION_I32: decode(value.value.i32, (int32_t) FBP_BBUF_DECODE_U32_LE(payload)); break;
        case FBP_UNION_I64: decode(value.value.i64, (int64_t) FBP_BBUF_DECODE_U64_LE(payload)); break;
        default:
            FBP_LOGW("unsupported type: %d", (int) type);
            return;
    }
    FBP_LOGI("pubsub_port recv %s%s", topic,
             (value.flags & FBP_PUBSUB_SFLAG_RETAIN) ? " | retain" : "");
    fbp_pubsub_publish(self->pubsub, topic, &value,
                       (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static void on_connected(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                         uint8_t port_data,
                         uint8_t *msg, uint32_t msg_size) {
    (void) seq;
    (void) port_data;
    if (msg_size < 2) {
        FBP_LOGW("invalid connected message");
        return;
    }
    if (msg[0]) {
        FBP_LOGW("connected error: %d", (int) msg[0]);
        return;
    }
    switch (self->fsm.state) {
        case ST_UPDATE_RECV:
            if (0 == msg[1]) {
                emit_event(self, EV_END_TOPIC);
            } else {
                FBP_LOGW("connected rsp when req expected");
            }
            break;
        case ST_CONN_RSP_RECV:
            if (1 == msg[1]) {
                emit_event(self, EV_RECV);
            } else {
                FBP_LOGW("connected req when rsp expected");
            }
            break;
        default:
            FBP_LOGW("unexpected connected message");
            break;
    }
}

void fbp_pubsubp_on_recv(struct fbp_pubsubp_s *self,
                          uint8_t port_id,
                          enum fbp_transport_seq_e seq,
                          uint8_t port_data,
                          uint8_t *msg, uint32_t msg_size) {
    if (!self->pubsub) {
        return;
    }
    if (port_id != self->port_id) {
        FBP_LOGW("port_id mismatch: %d != %d", (int) port_id, (int) self->port_id);
        return;
    }

    switch (port_data & FBP_PUBSUBP_PORT_DATA_MSG_MASK) {
        case FBP_PUBSUBP_MSG_NEGOTIATE: on_negotiate(self, seq, port_data, msg, msg_size); break;
        case FBP_PUBSUBP_MSG_TOPIC_LIST: on_topic_list(self, seq, port_data, msg, msg_size); break;
        case FBP_PUBSUBP_MSG_TOPIC_ADD: on_topic_add(self, seq, port_data, msg, msg_size); break;
        case FBP_PUBSUBP_MSG_TOPIC_REMOVE: on_topic_remove(self, seq, port_data, msg, msg_size); break;
        case FBP_PUBSUBP_MSG_PUBLISH: on_publish(self, seq, port_data, msg, msg_size); break;
        case FBP_PUBSUBP_MSG_CONNECTED: on_connected(self, seq, port_data, msg, msg_size); break;
        default:
            FBP_LOGW("Unsupported server message: 0x%04x", port_data);
    }
}

static inline uint8_t topic_retain_bit(struct fbp_pubsubp_s *self) {
    if (self->mode && (self->source == 0)) {
        return FBP_PUBSUBP_PORT_DATA_RETAIN_BIT;
    } else {
        return 0;
    }
}

int32_t send_topic_list(struct fbp_pubsubp_s * self) {
    struct fbp_union_s value;
    fbp_pubsub_query(self->pubsub, FBP_PUBSUB_TOPIC_LIST, &value);

    uint32_t sz = value.size ? value.size : ((uint32_t) (strlen(value.value.str) + 1));
    uint8_t port_data = FBP_PUBSUBP_MSG_TOPIC_LIST | topic_retain_bit(self);
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              port_data, (const uint8_t *) value.value.str, sz, 0);
}

uint8_t fbp_pubsubp_on_update(struct fbp_pubsubp_s *self,
                              const char * topic, const struct fbp_union_s * value) {
    uint8_t port_data = 0;
    if (!self->transport) {
        return 0;
    } else if (topic[0] == '_') {
        if (0 == strcmp(self->feedback_topic, topic)) {
            FBP_LOGI("fbp_pubsubp_on_update end topic");
            emit_event(self, EV_END_TOPIC);
            return 0;
        } else if (value->type != FBP_UNION_STR) {
            return 0;  // ignore
        } else if (self->mode == 0) {
            return 0;  // ignore server
        } else if (0 == strcmp(FBP_PUBSUB_TOPIC_LIST, topic)) {
            return 0;  // topic list handled explicitly, skip here
        } else if (0 == strcmp(FBP_PUBSUB_TOPIC_ADD, topic)) {
            uint32_t sz = value->size ? value->size : ((uint32_t) (strlen(value->value.str) + 1));
            port_data = FBP_PUBSUBP_MSG_TOPIC_ADD | topic_retain_bit(self);
            return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                                      port_data, (const uint8_t *) value->value.str, sz, FBP_PUBSUBP_TIMEOUT_MS);
        } else if (0 == strcmp(FBP_PUBSUB_TOPIC_REMOVE, topic)) {
            uint32_t sz = value->size ? value->size : ((uint32_t) (strlen(value->value.str) + 1));
            port_data = FBP_PUBSUBP_MSG_TOPIC_REMOVE;
            return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                                      port_data, (const uint8_t *) value->value.str, sz, FBP_PUBSUBP_TIMEOUT_MS);
        } else {
            return 0;  // do not forward any other "_" topics.
        }
    } else if (self->fsm.state <= ST_TOPIC_LIST) {
        FBP_LOGW("fbp_pubsubp_on_update before ready");
        return 0;
    }

    bool retain = (value->flags & FBP_UNION_FLAG_RETAIN) != 0;
    FBP_LOGI("port publish %s%s", topic, retain ? " | retain" : "");
    uint8_t topic_len = 0;
    char * p = (char *) (self->msg + 3);
    while (*topic) {
        if (topic_len >= (FBP_PUBSUB_TOPIC_LENGTH_MAX - 1)) {
            FBP_LOGW("topic too long");
            return FBP_ERROR_PARAMETER_INVALID;
        }
        *p++ = *topic++;
        ++topic_len;
    }
    *p++ = 0;       // add string terminator
    ++topic_len;
    port_data = FBP_PUBSUBP_MSG_PUBLISH | (retain ? FBP_PUBSUBP_PORT_DATA_RETAIN_BIT : 0);
    self->msg[0] = value->type & 0x1f;
    self->msg[1] = 0; // reserved
    self->msg[2] = (topic_len & 0x1f);
    uint8_t payload_sz_max = (uint8_t) (sizeof(self->msg) - ((uint8_t *) (p + 3) - self->msg));
    uint8_t payload_sz = 0;
    uint8_t * payload_sz_ptr = (uint8_t *) p++;
    if (payload_sz_max < 8) {
        FBP_LOGW("payload full");
        return FBP_ERROR_PARAMETER_INVALID;
    }

    switch (value->type) {
        case FBP_UNION_NULL:
            break;
        case FBP_UNION_STR:  // intentional fall-through
        case FBP_UNION_JSON: {
            const char * s = value->value.str;
            while (*s) {
                if (payload_sz >= (payload_sz_max - 1)) {
                    FBP_LOGW("payload full");
                    return FBP_ERROR_PARAMETER_INVALID;
                }
                *p++ = *s++;
                ++payload_sz;
            }
            *p++ = 0;       // add string terminator
            ++payload_sz;
            break;
        }
        case FBP_UNION_BIN: {
            if (payload_sz_max < value->size) {
                FBP_LOGW("payload full");
                return FBP_ERROR_PARAMETER_INVALID;
            }
            const uint8_t * s = value->value.bin;
            for (uint8_t sz = 0; sz < value->size; ++sz) {
                *p++ = *s++;
                ++payload_sz;
            }
            break;
        }
        case FBP_UNION_F32: FBP_BBUF_ENCODE_U32_LE(p, value->value.u32); payload_sz = 4; break;  // u32 intentional
        case FBP_UNION_F64: FBP_BBUF_ENCODE_U64_LE(p, value->value.u64); payload_sz = 8; break;  // u64 intentional
        case FBP_UNION_U8: p[0] = value->value.u8; payload_sz = 1; break;
        case FBP_UNION_U16: FBP_BBUF_ENCODE_U16_LE(p, value->value.u16); payload_sz = 2; break;
        case FBP_UNION_U32: FBP_BBUF_ENCODE_U32_LE(p, value->value.u32); payload_sz = 4; break;
        case FBP_UNION_U64: FBP_BBUF_ENCODE_U64_LE(p, value->value.u64); payload_sz = 8; break;
        case FBP_UNION_I8: p[0] = (uint8_t) value->value.i8; payload_sz = 1; break;
        case FBP_UNION_I16: FBP_BBUF_ENCODE_U16_LE(p, (uint16_t) value->value.i16); payload_sz = 2; break;
        case FBP_UNION_I32: FBP_BBUF_ENCODE_U32_LE(p, (uint32_t) value->value.i32); payload_sz = 4; break;
        case FBP_UNION_I64: FBP_BBUF_ENCODE_U64_LE(p, (uint64_t) value->value.i64); payload_sz = 8; break;
        default:
            FBP_LOGW("unsupported type: %d", (int) value->type);
            return FBP_ERROR_PARAMETER_INVALID;
    }
    *payload_sz_ptr = (uint8_t) payload_sz;
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              port_data, self->msg, 4 + topic_len + payload_sz, FBP_PUBSUBP_TIMEOUT_MS);
}

#define ON_ENTER(fsm) \
    (void) event; \
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) fsm; \
    timeout_set(self, 1000); \
    tick_clear(self)

static inline void unsubscribe(struct fbp_pubsubp_s * self) {
    fbp_pubsub_unsubscribe_from_all(self->pubsub,
                                    (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static fbp_fsm_state_t on_enter_disconnected(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    timeout_clear(self);
    unsubscribe(self);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_start(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) fsm;
    if (self->mode) {
        ++self->client_connection_count;
        self->server_connection_count = 0;
    } else {
        ++self->server_connection_count;
        self->client_connection_count = 0;
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_send_tick(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) fsm;
    int32_t rc = 0;
    switch (self->fsm.state) {
        case ST_NEGOTIATE_REQ: rc = send_negotiate_req(self); break;
        case ST_NEGOTIATE_RSP: rc = send_negotiate_rsp(self); break;
        case ST_TOPIC_LIST: rc = send_topic_list(self); break;
        case ST_CONN_REQ_SEND: rc = send_conn_req(self); break;
        case ST_CONN_RSP_SEND: rc = send_conn_rsp(self); break;
        default:
            FBP_LOGW("unsupported send state");
            return FBP_STATE_NULL;
    }
    if (0 == rc) {
        emit_event(self, EV_SENT);
    } else {
        tick_set(self, 2);  // retry
    }
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_negotiate_req(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    self->source = 0;
    unsubscribe(self);
    if (self->mode == 0) {
        fbp_pubsub_subscribe(self->pubsub, self->feedback_topic, 0,
                             (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
    }
    return on_send_tick(fsm, event);
}

static fbp_fsm_state_t on_enter_client_negotiate_rsp(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    unsubscribe(self);
    return on_send_tick(fsm, event);
}

static fbp_fsm_state_t on_enter_send(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    return on_send_tick(fsm, event);
}

static fbp_fsm_state_t on_enter_recv_wait(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_update_send(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    if (self->mode) {
        uint8_t flags = FBP_PUBSUB_SFLAG_RSP;
        if (self->source) {
            flags |= FBP_PUBSUB_SFLAG_RETAIN;
        }
        fbp_pubsub_subscribe(self->pubsub, "", flags, (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
    } else {
        // already subscribed from topic list
    }
    publish_feedback_topic(self);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_update_recv(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    timeout_set(self, 5000);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t on_enter_connected(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    ON_ENTER(fsm);
    timeout_clear(self);
    fbp_transport_event_inject(self->transport, FBP_DL_EV_APP_CONNECTED);
    return FBP_STATE_ANY;
}

static fbp_fsm_state_t if_server_is_source(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) fsm;
    return (self->source == 0) ? FBP_STATE_ANY : FBP_STATE_SKIP;
}

static fbp_fsm_state_t if_client_is_source(struct fbp_fsm_s * fsm, fbp_fsm_event_t event) {
    (void) event;
    struct fbp_pubsubp_s * self = (struct fbp_pubsubp_s *) fsm;
    return (self->source == 1) ? FBP_STATE_ANY : FBP_STATE_SKIP;
}

static const struct fbp_fsm_transition_s client_transitions[] = {  // priority encoded
    {ST_DISCONNECTED, ST_NEGOTIATE_REQ, EV_TRANSPORT_CONNECT, on_start},
    {ST_NEGOTIATE_REQ, ST_NEGOTIATE_RSP, EV_RECV_NEGOTIATE, NULL},
    {ST_NEGOTIATE_RSP, ST_TOPIC_LIST, EV_SENT, NULL},
    {ST_TOPIC_LIST, ST_UPDATE_RECV, EV_SENT, if_server_is_source},
    {ST_TOPIC_LIST, ST_UPDATE_SEND, EV_SENT, if_client_is_source},

    {ST_UPDATE_RECV, ST_CONN_RSP_SEND, EV_END_TOPIC, NULL},
    {ST_CONN_RSP_SEND, ST_CONNECTED, EV_SENT, NULL},

    {ST_UPDATE_SEND, ST_CONN_REQ_SEND, EV_END_TOPIC, NULL},
    {ST_CONN_REQ_SEND, ST_CONN_RSP_RECV, EV_SENT, NULL},
    {ST_CONN_RSP_RECV, ST_CONNECTED, EV_RECV, NULL},

    {ST_NEGOTIATE_RSP, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_TOPIC_LIST, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_CONN_REQ_SEND, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_CONN_RSP_SEND, FBP_STATE_NULL, EV_TICK, on_send_tick},

    {FBP_STATE_ANY, ST_NEGOTIATE_RSP, EV_RECV_NEGOTIATE, NULL},
    {FBP_STATE_ANY, ST_DISCONNECTED, EV_DISCONNECT, NULL},
    {FBP_STATE_ANY, ST_NEGOTIATE_REQ, EV_TIMEOUT,        NULL},
    {FBP_STATE_ANY, ST_DISCONNECTED, FBP_EVENT_RESET,   NULL},
};

static const struct fbp_fsm_transition_s server_transitions[] = {  // priority encoded
    {ST_DISCONNECTED, ST_NEGOTIATE_REQ, EV_TRANSPORT_CONNECT, on_start},
    {ST_NEGOTIATE_REQ, ST_NEGOTIATE_RSP, EV_SENT, NULL},
    {ST_NEGOTIATE_RSP, ST_TOPIC_LIST, EV_RECV_NEGOTIATE, NULL},
    {ST_TOPIC_LIST, ST_UPDATE_RECV, EV_RECV, if_client_is_source},
    {ST_TOPIC_LIST, ST_UPDATE_SEND, EV_RECV, if_server_is_source},

    {ST_UPDATE_RECV, ST_CONN_RSP_SEND, EV_END_TOPIC, NULL},
    {ST_CONN_RSP_SEND, ST_CONNECTED, EV_SENT, NULL},

    {ST_UPDATE_SEND, ST_CONN_REQ_SEND, EV_END_TOPIC, NULL},
    {ST_CONN_REQ_SEND, ST_CONN_RSP_RECV, EV_SENT, NULL},
    {ST_CONN_RSP_RECV, ST_CONNECTED, EV_RECV, NULL},

    {ST_NEGOTIATE_RSP, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_TOPIC_LIST, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_CONN_REQ_SEND, FBP_STATE_NULL, EV_TICK, on_send_tick},
    {ST_CONN_RSP_SEND, FBP_STATE_NULL, EV_TICK, on_send_tick},

    {FBP_STATE_ANY, ST_DISCONNECTED, EV_DISCONNECT, NULL},
    {FBP_STATE_ANY, ST_NEGOTIATE_REQ, EV_TIMEOUT,        NULL},
    {FBP_STATE_ANY, ST_DISCONNECTED, FBP_EVENT_RESET,   NULL},
};

static const struct fbp_fsm_state_s client_states[] = {
    {ST_DISCONNECTED,   "disconnected",  on_enter_disconnected, NULL},
    {ST_NEGOTIATE_REQ,  "negotiate_req", on_enter_negotiate_req, NULL},
    {ST_NEGOTIATE_RSP,  "negotiate_rsp", on_enter_client_negotiate_rsp, NULL},
    {ST_TOPIC_LIST,     "topic_list",    on_enter_send, NULL},
    {ST_UPDATE_SEND,    "update_send",   on_enter_update_send, NULL},
    {ST_UPDATE_RECV,    "update_recv",   on_enter_update_recv, NULL},
    {ST_CONN_REQ_SEND,  "conn_req_send", on_enter_send, NULL},
    {ST_CONN_RSP_SEND,  "conn_rsp_send", on_enter_send, NULL},
    {ST_CONN_RSP_RECV,  "conn_rsp_recv", on_enter_recv_wait, NULL},
    {ST_CONNECTED,      "connected",     on_enter_connected, NULL},
};

static const struct fbp_fsm_state_s server_states[] = {
    {ST_DISCONNECTED,   "disconnected",  on_enter_disconnected, NULL},
    {ST_NEGOTIATE_REQ,  "negotiate_req", on_enter_negotiate_req, NULL},
    {ST_NEGOTIATE_RSP,  "negotiate_rsp", on_enter_recv_wait, NULL},
    {ST_TOPIC_LIST,     "topic_list",    on_enter_recv_wait, NULL},
    {ST_UPDATE_SEND,    "update_send",   on_enter_update_send, NULL},
    {ST_UPDATE_RECV,    "update_recv",   on_enter_update_recv, NULL},
    {ST_CONN_REQ_SEND,  "conn_req_send", on_enter_send, NULL},
    {ST_CONN_RSP_SEND,  "conn_rsp_send", on_enter_send, NULL},
    {ST_CONN_RSP_RECV,  "conn_rsp_recv", on_enter_recv_wait, NULL},
    {ST_CONNECTED,      "connected",     on_enter_connected, NULL},
};

struct fbp_pubsubp_s * fbp_pubsubp_initialize(
        struct fbp_pubsub_s * pubsub,
        struct fbp_evm_api_s * evm,
        enum fbp_pubsubp_mode_e mode) {
    struct fbp_pubsubp_s * self = fbp_alloc_clr(sizeof(struct fbp_pubsubp_s));
    FBP_ASSERT_ALLOC(self);
    bool is_client = (mode == FBP_PUBSUBP_MODE_UPSTREAM);
    self->mode = mode;
    self->pubsub = pubsub;
    self->evm = *evm;
    construct_feedback_topic(self);

    self->fsm.name = is_client ? "pubsubp_client" : "pubsubp_server";
    self->fsm.state = ST_DISCONNECTED;
    self->fsm.states = is_client ? client_states : server_states;
    self->fsm.states_count = is_client ? FBP_ARRAY_SIZE(client_states): FBP_ARRAY_SIZE(server_states);
    self->fsm.transitions = is_client ? client_transitions : server_transitions;
    self->fsm.transitions_count = is_client ? FBP_ARRAY_SIZE(client_transitions) : FBP_ARRAY_SIZE(server_transitions);
    self->fsm.event_name_fn = event_to_str;
    self->fsm.reentrant = 0;
    fbp_fsm_reset(&self->fsm);
    return self;
}

void fbp_pubsubp_finalize(struct fbp_pubsubp_s * self) {
    if (self) {
        fbp_free(self);
    }
}

const char * fbp_pubsubp_feedback_topic(struct fbp_pubsubp_s *self) {
    return self->feedback_topic;
}
