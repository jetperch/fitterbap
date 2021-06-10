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
#include "fitterbap/comm/wave_sink_port.h"
#include "fitterbap/cstr.h"
#include "fitterbap/memory/bbuf.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/topic.h"
#include <inttypes.h>


static const char META[] = "{\"type\":\"waveform\"}";


struct fbp_wavep_s {
    struct fbp_port_api_s api;
    struct fbp_topic_s data_topic;
    struct fbp_pubsub_s * pubsub;
    uint8_t port_id;
};

static int32_t initialize(struct fbp_port_api_s * api, const struct fbp_port_config_s * config) {
    struct fbp_wavep_s * self = (struct fbp_wavep_s *) api;
    self->pubsub = config->pubsub;
    self->port_id = config->port_id;
    fbp_topic_set(&self->data_topic, config->topic_prefix.topic);
    fbp_topic_append(&self->data_topic, "data");
    return 0;
}

static int32_t finalize(struct fbp_port_api_s * api) {
    struct fbp_wavep_s * self = (struct fbp_wavep_s *) api;
    fbp_free(self);
    return 0;
}

static void on_event(void *user_data, enum fbp_dl_event_e event) {
    struct fbp_wavep_s * self = (struct fbp_wavep_s *) user_data;
    (void) self;
    (void) event;
}

static void on_recv(void *user_data,
                    uint8_t port_id,
                    enum fbp_transport_seq_e seq,
                    uint8_t port_data,
                    uint8_t *msg, uint32_t msg_size) {
    struct fbp_wavep_s *self = (struct fbp_wavep_s *) user_data;
    if (port_id != self->port_id) {
        FBP_LOGW("port_id mismatch: %d != %d", (int) port_id, (int) self->port_id);
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("only single seq supported: %d", (int) seq);
        return;
    }
    if (port_data & 0x80) {  // data message
        if (msg_size < 4) {
            FBP_LOGW("data message too short");
            return;
        }
        if (port_data & 0x7f) {
            FBP_LOGE("data compression not yet supported");
            //uint32_t sample_id = FBP_BBUF_DECODE_U32_LE(msg);
            return;
        }
        FBP_LOGD1("port sz=%d", (int) msg_size);
        fbp_pubsub_publish(self->pubsub, self->data_topic.topic, &fbp_union_bin(msg, msg_size), NULL, NULL);
    } else {
        int msg_type = (int) ((port_data >> 4) & 0x0f);
        switch (msg_type) {
            default:
                FBP_LOGW("unsupported waveform message type: %d", msg_type);
                break;
        }
    }
}

struct fbp_port_api_s * fbp_wave_sink_factory() {
    struct fbp_wavep_s * self = fbp_alloc_clr(sizeof(struct fbp_wavep_s));
    FBP_ASSERT_ALLOC(self);
    self->api.meta = META;
    self->api.initialize = initialize;
    self->api.finalize = finalize;
    self->api.on_event = on_event;
    self->api.on_recv = on_recv;
    return &self->api;
}
