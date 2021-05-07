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
#include <inttypes.h>


const static char META[] = "{\"type\":\"waveform\"}";


struct fbp_wavep_s {
    struct fbp_port_api_s api;
    char data_topic[FBP_PUBSUB_TOPIC_LENGTH_MAX];
};

static int32_t initialize(struct fbp_port_api_s * api) {
    struct fbp_wavep_s * self = (struct fbp_wavep_s *) api;
    if (fbp_cstr_join(self->data_topic, self->api.topic_prefix, "data", sizeof(self->data_topic))) {
        FBP_LOGE("could not construct data topic");
        return FBP_ERROR_PARAMETER_INVALID;
    }
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
                    uint16_t port_data,
                    uint8_t *msg, uint32_t msg_size) {
    struct fbp_wavep_s *self = (struct fbp_wavep_s *) user_data;
    if (port_id != self->api.port_id) {
        FBP_LOGW("port_id mismatch: %d != %d", (int) port_id, (int) self->api.port_id);
        return;
    }
    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("only single seq supported: %d", (int) seq);
        return;
    }
    if (port_data & 0x8000) {  // data message
        if (msg_size < 4) {
            FBP_LOGW("data message too short");
            return;
        }
        if (port_data & 0xff) {
            FBP_LOGE("data compression not yet supported");
            //uint32_t sample_id = FBP_BBUF_DECODE_U32_LE(msg);
            return;
        }
        FBP_LOGI("port sz=%d", (int) msg_size);
        fbp_pubsub_publish(self->api.pubsub, self->data_topic, &fbp_union_bin(msg, msg_size), NULL, NULL);
    } else {
        int msg_type = (int) (port_data >> 12);
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
