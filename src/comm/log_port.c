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
#include "fitterbap/collections/list.h"
#include "fitterbap/memory/bbuf.h"
#include "fitterbap/os/task.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include "tinyprintf.h"
#include <stdarg.h>


struct msg_s {
    struct fbp_list_s item;
    struct fbp_logp_msg_buffer_s msg;
    uint16_t length;
};

struct logp_s {
    struct fbp_port_api_s api;
    struct fbp_logp_config_s config;
    uint8_t is_connected;
    fbp_logp_on_recv recv_fn;
    void * recv_user_data;
    struct fbp_list_s msg_free;
    struct fbp_list_s msg_pend;
    struct msg_s * msg_alloc_ptr;
};

static const char META[] = "{\"type\":\"log\", \"name\":\"log\"}";

static inline void lock(struct logp_s * self) {
    fbp_os_mutex_lock(self->config.mutex);
}

static inline void unlock(struct logp_s * self) {
    fbp_os_mutex_unlock(self->config.mutex);
}

static int32_t initialize(struct fbp_port_api_s * api) {
    struct logp_s * self = (struct logp_s *) api;
    (void) self;
    return 0;
}

static int32_t finalize(struct fbp_port_api_s * api) {
    struct logp_s * self = (struct logp_s *) api;
    if (self) {
        fbp_os_mutex_t mutex = self->config.mutex;
        if (mutex) {
            lock(self);
        }
        if (self->msg_alloc_ptr) {
            fbp_free(self->msg_alloc_ptr);
            self->msg_alloc_ptr = 0;
        }
        fbp_list_initialize(&self->msg_free);
        fbp_list_initialize(&self->msg_pend);
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
        }
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
    struct fbp_logp_record_s record;
    if (!self->recv_fn) {
        return;  // no callback, no worries!
    } else if (port_id != self->api.port_id) {
        return;
    } else if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        return;
    } else if (msg_size < (sizeof(struct fbp_logp_header_s) + 2)) {
        return;
    } else if (msg[0] != FBP_LOGP_VERSION) {
        return;
    }
    record.timestamp = FBP_BBUF_DECODE_U64_LE(msg + 8);
    record.level = msg[4] & 0x0f;
    record.origin_prefix = msg[1];
    record.origin_thread = msg[5];
    record.line = (((uint32_t) (msg[4] & 0xf0)) << 12) | FBP_BBUF_DECODE_U16_LE(msg + 2);

    p = (char *) (msg + 16);
    record.filename = p;
    for (int i = 0; (i < FBP_LOGP_FILENAME_SIZE_MAX) && (*p) && (*p != FBP_LOGP_SEP) && (p < p_end); ++i) {
        p++;
    }
    if (*p != FBP_LOGP_SEP) {
        // something is wrong
    }
    *p++ = 0;
    record.message = p;
    for (int i = 0; (i < FBP_LOGP_MESSAGE_SIZE_MAX) && (*p) && (p < p_end); ++i) {
        p++;
    }
    *p = 0;

    self->recv_fn(self->recv_user_data, &record);
}

static const char * find_basename(const char * filename) {
    const char * p = filename;
    while (*filename) {
        if (*filename == '/') {
            p = filename + 1;
        }
        ++filename;
    }
    return p;
}

int32_t fbp_logp_publish(struct fbp_port_api_s * api, uint8_t level, const char * filename, uint32_t line, const char * format, ...) {
    int32_t rc = 0;
    va_list args;
    char * p;
    struct logp_s * self = (struct logp_s *) api;
    if ((level > 0x0f) || (line >= 0x100000)) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if (!self || !self->is_connected) {
        return FBP_ERROR_UNAVAILABLE;
    }
    filename = find_basename(filename);

    lock(self);
    if (fbp_list_is_empty(&self->msg_free)) {
        rc = FBP_ERROR_FULL;
    } else {
        struct msg_s * msg = (struct msg_s *) fbp_list_remove_head(&self->msg_free);
        msg->msg.header.version = FBP_LOGP_VERSION;
        msg->msg.header.origin_prefix = self->api.topic_prefix[0];
        msg->msg.header.line = line;
        msg->msg.header.level = (level & 0x0f) | ((line >> 12) & 0xf0);
        msg->msg.header.origin_thread = 0;
        msg->msg.header.rsv8_1 = 0;
        msg->msg.header.rsv8_2 = 0;
        msg->msg.header.timestamp = fbp_time_utc();
        p = msg->msg.data;
        for (int i = 0; (i < FBP_LOGP_FILENAME_SIZE_MAX) && (*filename); ++i) {
            *p++ = *filename++;
        }
        *p++ = FBP_LOGP_SEP;
        va_start(args, format);
        p += tfp_vsnprintf(p, FBP_LOGP_MESSAGE_SIZE_MAX, format, args);
        *p++ = 0;
        va_end(args);
        msg->length = p - (char *) &msg->msg;

        fbp_list_add_tail(&self->msg_pend, &msg->item);
        if (self->config.on_publish_fn) {
            self->config.on_publish_fn(self->config.on_publish_user_data);
        }
    }
    unlock(self);
    return rc;
}

int32_t fbp_logp_publish_record(struct fbp_port_api_s * api, struct fbp_logp_record_s * record) {
    int32_t rc = 0;
    char * p;
    struct logp_s * self = (struct logp_s *) api;
    if (!self || !self->is_connected) {
        return FBP_ERROR_UNAVAILABLE;
    }
    const char * filename = find_basename(record->filename);

    lock(self);
    if (fbp_list_is_empty(&self->msg_free)) {
        rc = FBP_ERROR_FULL;
    } else {
        struct msg_s * msg = (struct msg_s *) fbp_list_remove_head(&self->msg_free);
        msg->msg.header.version = FBP_LOGP_VERSION;
        msg->msg.header.origin_prefix = record->origin_prefix;
        msg->msg.header.line = record->line;
        msg->msg.header.level = (record->level & 0x0f) | ((record->line >> 12) & 0xf0);
        msg->msg.header.origin_thread = record->origin_thread;
        msg->msg.header.rsv8_1 = 0;
        msg->msg.header.rsv8_2 = 0;
        msg->msg.header.timestamp = record->timestamp;

        p = msg->msg.data;
        for (int i = 0; (i < FBP_LOGP_FILENAME_SIZE_MAX) && (*filename); ++i) {
            *p++ = *filename++;
        }
        *p++ = FBP_LOGP_SEP;
        for (int i = 0; (i < FBP_LOGP_MESSAGE_SIZE_MAX) && (*record->message); ++i) {
            *p++ = *record->message++;
        }
        *p++ = 0;
        msg->length = p - (char *) &msg->msg;
        fbp_list_add_tail(&self->msg_pend, &msg->item);
        if (self->config.on_publish_fn) {
            self->config.on_publish_fn(self->config.on_publish_user_data);
        }
    }
    unlock(self);
    return rc;
}

void fbp_logp_recv_register(struct fbp_port_api_s * api, fbp_logp_on_recv cbk_fn, void * cbk_user_data) {
    struct logp_s * self = (struct logp_s *) api;
    lock(self);
    self->recv_fn = 0;
    self->recv_user_data = cbk_user_data;
    self->recv_fn = cbk_fn;
    unlock(self);
}

int32_t fbp_logp_process(struct fbp_port_api_s * api) {
    struct logp_s * self = (struct logp_s *) api;
    struct msg_s * msg;
    int32_t rc;
    while (self->api.transport) {
        lock(self);
        if (fbp_list_is_empty(&self->msg_pend)) {
            unlock(self);
            return 0;
        }
        msg = (struct msg_s *) fbp_list_remove_head(&self->msg_pend);
        unlock(self);
        rc = fbp_transport_send(self->api.transport, self->api.port_id,
                                FBP_TRANSPORT_SEQ_SINGLE, 0,
                                (uint8_t *) &msg->msg.header, msg->length, 0);
        if (rc) {
            lock(self);
            fbp_list_add_head(&self->msg_pend, &msg->item);
            unlock(self);
            return rc;
        }
    }
    return 0;
}

FBP_API struct fbp_port_api_s * fbp_logp_factory(struct fbp_logp_config_s const * config) {
    struct logp_s * self = fbp_alloc_clr(sizeof(struct logp_s));
    self->config = *config;
    fbp_list_initialize(&self->msg_free);
    fbp_list_initialize(&self->msg_pend);

    self->msg_alloc_ptr = fbp_alloc_clr(config->msg_buffers_max * sizeof(struct msg_s));
    for (uint32_t i = 0; i < config->msg_buffers_max; ++i) {
        struct msg_s * msg = &self->msg_alloc_ptr[i];
        fbp_list_initialize(&msg->item);
        fbp_list_add_tail(&self->msg_free, &msg->item);
    }

    self->api.meta = META;
    self->api.initialize = initialize;
    self->api.finalize = finalize;
    self->api.on_event = on_transport_event;
    self->api.on_recv = on_transport_recv;

    return &self->api;
}
