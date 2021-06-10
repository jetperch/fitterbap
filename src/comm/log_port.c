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

#include "fitterbap/comm/log_port.h"
#include "fitterbap/log.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/os/task.h"
#include "fitterbap/ec.h"
#include <stdarg.h>

static const char LEVEL_TOPIC[] = "level";

static const char LEVEL_META[] =
    "{"
        "\"dtype\":\"u32\","
        "\"brief\":\"Log level filter.\","
        "\"default\":5,"
        "\"options\":["
            "[0,\"emergency\"],"
            "[1,\"alert\"],"
            "[2,\"critical\"],"
            "[3,\"error\"],"
            "[4,\"warning\"],"
            "[5,\"notice\"],"
            "[6,\"info\"],"
            "[7,\"dbg1\"],"
            "[8,\"dbg2\"],"
            "[9,\"dbg3\"],"
            "[10,\"all\"]"
        "]"
    "}";

/**
 * @brief The full-sized logging message structure.
 */
struct buf_s {
    /// The message metadata.
    struct fbp_logh_header_s header;
    /// The filename and message strings.
    char data[FBP_LOGP_DATA_SIZE_MAX];
};

struct logp_s {
    struct fbp_port_api_s api;
    uint8_t is_connected;
    uint8_t level_filter;
    fbp_logp_publish_formatted pub_fn;
    void * pub_user_data;

    struct fbp_transport_s * transport;
    uint8_t port_id;
};

static const char META[] = "{\"type\":\"log\", \"name\":\"log\"}";

uint8_t on_log_level(void * user_data,
                     const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct logp_s * self = (struct logp_s *) user_data;
    if (value->type == FBP_UNION_U32) {
        if (value->value.u32 <= FBP_LOG_LEVEL_ALL) {
            self->level_filter = (uint8_t) value->value.u32;
            return 0;
        }
    } else if (value->type == FBP_UNION_U8) {
        if (value->value.u8 <= FBP_LOG_LEVEL_ALL) {
            self->level_filter = value->value.u8;
            return 0;
        }
    } else {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    return FBP_ERROR_PARAMETER_INVALID;
}

static int32_t initialize(struct fbp_port_api_s * api, const struct fbp_port_config_s * config) {
    struct logp_s * self = (struct logp_s *) api;
    struct fbp_topic_s topic;
    self->level_filter = FBP_LOG_LEVEL_NOTICE;
    fbp_topic_set(&topic, config->topic_prefix.topic);
    fbp_topic_append(&topic, LEVEL_TOPIC);
    fbp_pubsub_meta(config->pubsub, topic.topic, LEVEL_META);
    fbp_pubsub_subscribe(config->pubsub, topic.topic, 0, on_log_level, self);
    fbp_pubsub_publish(config->pubsub, topic.topic, &fbp_union_u8_r(self->level_filter), on_log_level, self);

    self->transport = config->transport;
    self->port_id = config->port_id;
    return 0;
}

static int32_t finalize(struct fbp_port_api_s * api) {
    struct logp_s * self = (struct logp_s *) api;
    if (self) {
        fbp_free(self);
    }
    return 0;
}

static void on_transport_event(void *user_data, enum fbp_dl_event_e event) {
    struct logp_s * self = (struct logp_s *) user_data;
    self->is_connected = (event == FBP_DL_EV_APP_CONNECTED) ? 1 : 0;
}

void on_transport_recv(void *user_data,
                       uint8_t port_id,
                       enum fbp_transport_seq_e seq,
                       uint8_t port_data,
                       uint8_t *msg, uint32_t msg_size) {
    (void) port_data;
    char * p;
    char * p_end = (char *) (msg + msg_size - 1);  // last char should be null string terminator
    struct logp_s * self = (struct logp_s *) user_data;
    struct fbp_logh_header_s header;
    if (!self || !self->pub_fn) {
        return;  // no callback, no worries!
    } else if (port_id != self->port_id) {
        return;
    } else if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        return;
    } else if (msg_size < (sizeof(header) + 2)) {
        return;
    } else if (msg[0] != FBP_LOGH_VERSION) {
        return;
    }
    memcpy(&header, msg, sizeof(header));
    if (header.level > self->level_filter) {
        return;
    }
    char * filename = (char *) msg + sizeof(header);
    p = filename;
    for (int i = 0; (i < FBP_LOGH_FILENAME_SIZE_MAX) && (*p) && (*p != FBP_LOGP_SEP) && (p < p_end); ++i) {
        p++;
    }
    if (*p == FBP_LOGP_SEP) {
        *p++ = 0;  // terminate filename string
    } else {
        // invalid format, process the best we can
    }
    char * message = p;
    for (int i = 0; (i < FBP_LOGH_MESSAGE_SIZE_MAX) && (*p) && (p < p_end); ++i) {
        p++;
    }
    *p = 0;

    self->pub_fn(self->pub_user_data, &header, filename, message);
}

int32_t fbp_logp_recv(void * user_data, struct fbp_logh_header_s const * header,
                      const char * filename, const char * message) {
    struct logp_s * self = (struct logp_s *) user_data;
    struct buf_s buf;
    char * p_start = (char *) &buf;
    char * p;
    if (!self || !self->is_connected) {
        return FBP_ERROR_UNAVAILABLE;  // discard
    }
    if (header->level > self->level_filter) {
        return 0;
    }
    buf.header = *header;
    p = buf.data;
    for (int i = 0; (i < FBP_LOGH_FILENAME_SIZE_MAX) && (*filename); ++i) {
        *p++ = *filename++;
    }
    *p++ = FBP_LOGP_SEP;
    for (int i = 0; (i < FBP_LOGH_MESSAGE_SIZE_MAX) && (*message); ++i) {
        *p++ = *message++;
    }
    *p++ = 0;

    return fbp_transport_send(self->transport, self->port_id, FBP_TRANSPORT_SEQ_SINGLE,
                              0, (uint8_t *) p_start, p - p_start, 0);
}

void fbp_logp_handler_register(struct fbp_port_api_s * api, fbp_logp_publish_formatted fn, void * user_data) {
    struct logp_s * self = (struct logp_s *) api;
    self->pub_fn = NULL;
    self->pub_user_data = user_data;
    self->pub_fn = fn;
}

FBP_API struct fbp_port_api_s * fbp_logp_factory() {
    struct logp_s * self = fbp_alloc_clr(sizeof(struct logp_s));
    self->api.meta = META;
    self->api.initialize = initialize;
    self->api.finalize = finalize;
    self->api.on_event = on_transport_event;
    self->api.on_recv = on_transport_recv;
    return &self->api;
}
