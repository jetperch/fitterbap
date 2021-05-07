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
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/cstr.h"
#include "fitterbap/os/task.h"

enum state_e {
    ST_TX_DISCONNECTED,
    ST_SERVER_TX_CONNECTED,     // sent MSG_CONN request, await response
    ST_CLIENT_WAIT_MSG_CONN,    // client connected, await MSG_CONN request
    ST_CLIENT_WAIT_TX_CONNECT,  // client received MSG_CONN, await TX connect
    ST_CLIENT_WAIT_PUBSUB,      // client subscribed to PubSub, wait for _/topic/list
    ST_SERVER_AWAIT_TOPIC_LIST, // MSG_CONN response received, await topics.
    ST_CONNECTED,
};


struct fbp_pubsubp_s {
    enum state_e state;
    uint64_t local_connection_count;
    uint64_t remote_connection_count;
    uint8_t port_id;
    uint8_t mode;
    struct fbp_pubsub_s * pubsub;
    struct fbp_transport_s * transport;
    uint8_t msg[FBP_FRAMER_PAYLOAD_MAX_SIZE];
};

const char FBP_PUBSUBP_META[] = "{\"type\":\"pubsub\", \"name\":\"pubsub\"}";


static bool is_restore_from_client(struct fbp_pubsubp_s *self) {
    uint64_t client_count;
    uint64_t server_count;
    if (self->mode == FBP_PUBSUBP_MODE_DOWNSTREAM) {
        client_count = self->remote_connection_count;
        server_count = self->local_connection_count;
    } else {
        client_count = self->local_connection_count;
        server_count = self->remote_connection_count;
    }
    if ((client_count == 1) && (server_count == 1)) {
        return true;
    }
    if (client_count > server_count) {
        return true;
    }
    return false;
}

struct fbp_pubsubp_s * fbp_pubsubp_initialize(struct fbp_pubsub_s * pubsub, enum fbp_pubsubp_mode_e mode) {
    struct fbp_pubsubp_s * self = fbp_alloc_clr(sizeof(struct fbp_pubsubp_s));
    FBP_ASSERT_ALLOC(self);
    self->mode = mode;
    self->pubsub = pubsub;
    return self;
}

void fbp_pubsubp_finalize(struct fbp_pubsubp_s * self) {
    if (self) {
        fbp_free(self);
    }
}

static void disconnect(struct fbp_pubsubp_s * self) {
    if (self->state != ST_TX_DISCONNECTED) {
        fbp_pubsub_unsubscribe_from_all(self->pubsub,
                                        (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
        self->state = ST_TX_DISCONNECTED;
    }
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

static void server_conn(struct fbp_pubsubp_s *self) {
    FBP_LOGI("server_conn");
    ++self->local_connection_count;
    // Send MSG_CONN request
    uint64_t msg[3];
    msg[0] = 0;
    msg[1] = self->local_connection_count;
    msg[2] = 0;
    fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                       FBP_PUBSUBP_MSG_CONN, (uint8_t *) msg, sizeof(msg), FBP_PUBSUBP_TIMEOUT_MS);
    self->state = ST_SERVER_TX_CONNECTED;
}

static void client_conn(struct fbp_pubsubp_s *self) {
    FBP_LOGI("client_conn");
    ++self->local_connection_count;
    // Send MSG_CONN response
    uint64_t msg[3];
    msg[0] = 1;
    msg[1] = self->remote_connection_count;
    msg[2] = self->local_connection_count;
    fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                       FBP_PUBSUBP_MSG_CONN, (uint8_t *) msg, sizeof(msg), FBP_PUBSUBP_TIMEOUT_MS);

    // subscribe
    uint8_t flags = FBP_PUBSUB_SFLAG_RSP;
    if (is_restore_from_client(self)) {
        flags |= FBP_PUBSUB_SFLAG_RETAIN;
    } else {
        // Request the topic list.
        fbp_pubsub_subscribe(self->pubsub, FBP_PUBSUB_TOPIC_LIST,
                             FBP_PUBSUB_SFLAG_RETAIN,
                             (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
    }
    fbp_pubsub_subscribe(self->pubsub, "", flags,
                         (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
    self->state = ST_CLIENT_WAIT_PUBSUB;
}

static void client_ev_conn(struct fbp_pubsubp_s *self) {
    switch (self->state) {
        case ST_TX_DISCONNECTED:
            FBP_LOGI("ST_CLIENT_WAIT_MSG_CONN");
            self->state = ST_CLIENT_WAIT_MSG_CONN;
            break;
        case ST_CLIENT_WAIT_MSG_CONN:
            FBP_LOGW("duplicate tx_connect");
            break;
        case ST_CLIENT_WAIT_TX_CONNECT:
            client_conn(self);
            break;
        default:
            FBP_LOGW("duplicate ev_conn");
            break;
    }
}

void fbp_pubsubp_on_event(struct fbp_pubsubp_s *self, enum fbp_dl_event_e event) {
    FBP_LOGI("fbp_pubsubp_on_event(%d) in %d", (int) event, (int) self->state);
    switch (event) {
        case FBP_DL_EV_RX_RESET_REQUEST:
            if (self->mode == FBP_PUBSUBP_MODE_UPSTREAM)  {
                disconnect(self);
            }
            break;
        case FBP_DL_EV_TX_DISCONNECTED:
            disconnect(self);
            break;
        case FBP_DL_EV_TX_CONNECTED:
            if (self->mode == FBP_PUBSUBP_MODE_UPSTREAM) {
                client_ev_conn(self);
            } else {  // downstream
                if (self->state == ST_TX_DISCONNECTED) {
                    server_conn(self);
                    self->state = ST_SERVER_TX_CONNECTED;
                }
            }
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

static void on_client_conn(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                           uint16_t port_data,
                           uint8_t *msg, uint32_t msg_size) {
    (void) port_data;  // unused by msg_conn
    if ((self->state != ST_TX_DISCONNECTED) && (self->state != ST_CLIENT_WAIT_MSG_CONN)) {
        FBP_LOGW("msg_conn in state %d", (int) self->state);
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    uint64_t v[3];
    if (msg_size != sizeof(v)) {
        FBP_LOGW("invalid msg_conn size: %d", (int) msg_size);
        return;
    }
    memcpy(v, msg, sizeof(v));
    if (v[0] != 0) {
        FBP_LOGW("invalid msg_conn request");
        return;
    }
    self->remote_connection_count = v[1];
    if (self->state == ST_CLIENT_WAIT_MSG_CONN) {
        client_conn(self);
    } else {
        self->state = ST_CLIENT_WAIT_TX_CONNECT;
    }
}

static void on_server_conn(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                           uint16_t port_data,
                           uint8_t *msg, uint32_t msg_size) {
    (void) port_data;  // unused by msg_conn
    if (self->state != ST_SERVER_TX_CONNECTED) {
        FBP_LOGW("msg_conn in state %d", (int) self->state);
        return;
    }
    uint64_t v[3];
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    if (msg_size != sizeof(v)) {
        FBP_LOGW("invalid msg_conn size: %d", (int) msg_size);
        return;
    }
    memcpy(v, msg, sizeof(v));
    if (v[0] != 1) {
        FBP_LOGW("invalid msg_conn response");
        return;
    }
    self->remote_connection_count = v[2];
    self->state = ST_SERVER_AWAIT_TOPIC_LIST;
}

static void on_server_topic_list(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                 uint16_t port_data,
                                 uint8_t *msg, uint32_t msg_size) {
    if (self->state == ST_CONNECTED) {
        // already connected, use add / remove
        return;
    } else if (self->state != ST_SERVER_AWAIT_TOPIC_LIST) {
        FBP_LOGW("topic list in state %d", (int) self->state);
        return;
    } else if ((seq != FBP_TRANSPORT_SEQ_SINGLE)  || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_list");
        return;
    }
    self->state = ST_CONNECTED;
    fbp_pubsub_subscribe(self->pubsub, "", FBP_PUBSUB_SFLAG_NOPUB | FBP_PUBSUB_SFLAG_REQ,
                         (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);

    uint8_t flags = 0;
    if (port_data & FBP_PUBSUBP_PORT_DATA_RETAIN_BIT) {
        flags |= FBP_PUBSUB_SFLAG_RETAIN;
    }

    char * topic = (char *) msg;
    char * end = topic;
    char end_ch;
    while (*topic) {
        while (*end && *end != FBP_PUBSUB_UNIT_SEP_CHR) {
            ++end;
        }
        end_ch = *end;
        *end = 0;
        fbp_pubsub_subscribe(self->pubsub, topic, flags,
                             (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
        if (end_ch) {
            topic = end + 1;
        } else {
            topic = end;
        }
        end = topic;
    }
}

static void on_server_topic_add(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                uint16_t port_data,
                                uint8_t *msg, uint32_t msg_size) {
    if ((seq != FBP_TRANSPORT_SEQ_SINGLE) || (msg_size > FBP_PUBSUB_TOPIC_LENGTH_MAX) || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_add");
        return;
    }
    uint8_t flags = 0;
    if (port_data & FBP_PUBSUBP_PORT_DATA_RETAIN_BIT) {
        flags |= FBP_PUBSUB_SFLAG_RETAIN;
    }
    fbp_pubsub_subscribe(self->pubsub, (char *) msg, flags,
                           (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static void on_server_topic_remove(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                                   uint16_t port_data,
                                   uint8_t *msg, uint32_t msg_size) {
    (void) port_data;
    if ((seq != FBP_TRANSPORT_SEQ_SINGLE) || (msg_size > FBP_PUBSUB_TOPIC_LENGTH_MAX) || !msg_size || msg[msg_size - 1]) {
        FBP_LOGW("invalid topic_remove");
        return;
    }
    fbp_pubsub_unsubscribe(self->pubsub, (char *) msg,
                           (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

static void on_publish(struct fbp_pubsubp_s *self, enum fbp_transport_seq_e seq,
                       uint16_t port_data,
                       uint8_t *msg, uint32_t msg_size) {
    if (self->state != ST_CONNECTED) {
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("invalid seq: %d", (int) seq);
        return;
    }
    uint8_t type = (port_data >> 8) & 0x1f;
    uint8_t flags = (port_data >> 13) & 0x07;
    if ((port_data & FBP_PUBSUBP_PORT_DATA_MSG_MASK) != FBP_PUBSUBP_MSG_PUBLISH) {
        FBP_LOGW("invalid port_data: %d", (int) port_data);
        return;
    }
    if (msg_size < 3) {  // topic len, topic null terminator, payload length
        FBP_LOGW("msg too small");
        return;
    }
    if (msg[0] > FBP_PUBSUB_TOPIC_LENGTH_MAX) {
        FBP_LOGW("topic too long");
        return;
    }
    uint8_t topic_len = msg[0];  // 32 max
    char * topic = (char *) (msg + 1);

    uint32_t sz = 1 + topic_len;  // topic length byte + topic bytes
    if (msg_size < (sz + 1)) {  // +1 for payload length
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
    fbp_pubsub_publish(self->pubsub, topic, &value,
                       (fbp_pubsub_subscribe_fn) fbp_pubsubp_on_update, self);
}

void fbp_pubsubp_on_recv(struct fbp_pubsubp_s *self,
                          uint8_t port_id,
                          enum fbp_transport_seq_e seq,
                          uint16_t port_data,
                          uint8_t *msg, uint32_t msg_size) {
    if (!self->pubsub) {
        return;
    }
    if (port_id != self->port_id) {
        FBP_LOGW("port_id mismatch: %d != %d", (int) port_id, (int) self->port_id);
        return;
    }

    if (self->mode == FBP_PUBSUBP_MODE_UPSTREAM) {
        switch (port_data & FBP_PUBSUBP_PORT_DATA_MSG_MASK) {
            case FBP_PUBSUBP_MSG_CONN: on_client_conn(self, seq, port_data, msg, msg_size); break;
            case FBP_PUBSUBP_MSG_PUBLISH: on_publish(self, seq, port_data, msg, msg_size); break;
            default:
                FBP_LOGW("Unsupported client message: 0x%04x", port_data);
        }
    } else {
        switch (port_data & FBP_PUBSUBP_PORT_DATA_MSG_MASK) {
            case FBP_PUBSUBP_MSG_CONN: on_server_conn(self, seq, port_data, msg, msg_size); break;
            case FBP_PUBSUBP_MSG_TOPIC_LIST: on_server_topic_list(self, seq, port_data, msg, msg_size); break;
            case FBP_PUBSUBP_MSG_TOPIC_ADD: on_server_topic_add(self, seq, port_data, msg, msg_size); break;
            case FBP_PUBSUBP_MSG_TOPIC_REMOVE: on_server_topic_remove(self, seq, port_data, msg, msg_size); break;
            case FBP_PUBSUBP_MSG_PUBLISH: on_publish(self, seq, port_data, msg, msg_size); break;
            default:
                FBP_LOGW("Unsupported server message: 0x%04x", port_data);
        }
    }
}

static inline uint16_t retain_bit(struct fbp_pubsubp_s *self) {
    return is_restore_from_client(self) ? 0 : FBP_PUBSUBP_PORT_DATA_RETAIN_BIT;
}

uint8_t fbp_pubsubp_on_update(struct fbp_pubsubp_s *self,
                               const char * topic, const struct fbp_union_s * value) {
    uint16_t port_data = 0;
    if (!self->transport) {
        return 0;
    }
    if (self->state == ST_CLIENT_WAIT_PUBSUB) {
        if (0 != strcmp(FBP_PUBSUB_TOPIC_LIST, topic)) {
            return 0; // not yet ready to handle updates
        }
        if (value->type != FBP_UNION_STR) {
            FBP_LOGE("unsupported topic list type: 0x%02x", value->type);
            return 0;
        }
        uint32_t sz = value->size ? value->size : ((uint32_t) (strlen(value->value.str) + 1));
        port_data = FBP_PUBSUBP_MSG_TOPIC_LIST | retain_bit(self);
        fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                           port_data, (const uint8_t *) value->value.str, sz, FBP_PUBSUBP_TIMEOUT_MS);
        self->state = ST_CONNECTED;
        return 0;
    } else if (self->state != ST_CONNECTED) {
        return 0; // not yet ready to handle updates.
    }

    if (topic[0] == '_') {
        if (value->type != FBP_UNION_STR) {
            // ignore
        } else if (0 == strcmp(FBP_PUBSUB_TOPIC_ADD, topic)) {
            uint32_t sz = value->size ? value->size : ((uint32_t) (strlen(value->value.str) + 1));
            port_data = FBP_PUBSUBP_MSG_TOPIC_ADD | retain_bit(self);
            fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                               port_data, (const uint8_t *) value->value.str, sz, FBP_PUBSUBP_TIMEOUT_MS);
        } else if (0 == strcmp(FBP_PUBSUB_TOPIC_REMOVE, topic)) {
            uint32_t sz = value->size ? value->size : ((uint32_t) (strlen(value->value.str) + 1));
            port_data = FBP_PUBSUBP_MSG_TOPIC_REMOVE;
            fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                               port_data, (const uint8_t *) value->value.str, sz, FBP_PUBSUBP_TIMEOUT_MS);
        } else {
            // do not forward "_" topics.
        }
        return 0;
    }

    FBP_LOGI("port publish %s", topic);
    uint8_t topic_len = 0;
    char * p = (char *) (self->msg + 1);
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

    uint8_t dtype = value->type & 0x1f;
    uint8_t dflag = (value->flags & 7);
    dflag &= ~FBP_UNION_FLAG_CONST;  // remove const, only applicable locally
    port_data = FBP_PUBSUBP_MSG_PUBLISH | (((uint16_t) ((dflag << 5) | dtype)) << 8);

    self->msg[0] = (topic_len & 0x1f);
    uint8_t payload_sz_max = (uint8_t) (sizeof(self->msg) - ((uint8_t *) (p + 1) - self->msg));
    uint8_t payload_sz = 0;
    uint8_t * hdr = (uint8_t *) p++;
    if (payload_sz_max < 8) {
        FBP_LOGW("payload full");
        return FBP_ERROR_PARAMETER_INVALID;
    }

    switch (dtype) {
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
        case FBP_UNION_F32: FBP_BBUF_ENCODE_U32_LE(p, *((uint32_t*) &value->value.f32)); payload_sz = 4; break;
        case FBP_UNION_F64: FBP_BBUF_ENCODE_U64_LE(p, *((uint64_t*) &value->value.f64)); payload_sz = 8; break;
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
    *hdr = (uint8_t) payload_sz;
    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              port_data, self->msg, 2 + topic_len + payload_sz, FBP_PUBSUBP_TIMEOUT_MS);
}
