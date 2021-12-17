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

#include "fitterbap/comm/stack.h"
#include "fitterbap/platform.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/topic.h"
#include <inttypes.h>


struct fbp_stack_s * fbp_stack_initialize(
        struct fbp_dl_config_s const * config,
        enum fbp_port0_mode_e port0_mode,
        const char * topic_prefix,
        struct fbp_evm_api_s * evm_api,
        struct fbp_dl_ll_s const * ll_instance,
        struct fbp_pubsub_s * pubsub,
        struct fbp_ts_s * timesync)  {

    struct fbp_port_config_s port_config;
    memset(&port_config, 0, sizeof(port_config));
    fbp_topic_set(&port_config.topic_prefix, topic_prefix);
    struct fbp_stack_s * self = fbp_alloc_clr(sizeof(struct fbp_stack_s));
    self->pubsub = pubsub;
    self->evm_api = *evm_api;

    self->framer = fbp_framer_initialize();
    if (!self->framer) {
        fbp_stack_finalize(self);
        return NULL;
    }

    self->dl = fbp_dl_initialize(config, evm_api, ll_instance, self->framer);
    if (!self->dl) {
        fbp_stack_finalize(self);
        return NULL;
    }

    self->transport = fbp_transport_initialize((fbp_transport_ll_send) fbp_dl_send, self->dl);
    if (!self->transport) {
        fbp_stack_finalize(self);
        return NULL;
    }

    struct fbp_dl_api_s dl_api = {
            .user_data = self->transport,
            .event_fn = (fbp_dl_event_fn) fbp_transport_on_event_cbk,
            .recv_fn = (fbp_dl_recv_fn) fbp_transport_on_recv_cbk,
    };
    fbp_dl_register_upper_layer(self->dl, &dl_api);

    self->port0 = fbp_port0_initialize(port0_mode, self->dl, evm_api, self->transport, fbp_transport_send,
                                        pubsub, port_config.topic_prefix.topic, timesync);
    if (!self->port0) {
        fbp_stack_finalize(self);
        return NULL;
    }
    if (fbp_transport_port_register(self->transport, 0,
                                     FBP_PORT0_META,
                                     (fbp_transport_event_fn) fbp_port0_on_event_cbk,
                                     (fbp_transport_recv_fn) fbp_port0_on_recv_cbk,
                                      self->port0)) {
        fbp_stack_finalize(self);
        return NULL;
    }

    enum fbp_pubsubp_mode_e pmode;
    switch (port0_mode) {
        case FBP_PORT0_MODE_CLIENT: pmode = FBP_PUBSUBP_MODE_UPSTREAM; break;
        case FBP_PORT0_MODE_SERVER: pmode = FBP_PUBSUBP_MODE_DOWNSTREAM; break;
        default:
            fbp_stack_finalize(self);
            return NULL;
    }
    self->pubsub_port = fbp_pubsubp_initialize(pubsub, evm_api, pmode);
    if (!self->pubsub_port) {
        fbp_stack_finalize(self);
        return NULL;
    }
    if (fbp_pubsubp_transport_register(self->pubsub_port, 1, self->transport)) {
        fbp_stack_finalize(self);
        return NULL;
    }

    fbp_topic_append(&port_config.topic_prefix, "2");
    port_config.pubsub = pubsub;
    port_config.transport = self->transport;
    port_config.port_id = 2;
    port_config.evm = *evm_api;
    self->logp = fbp_logp_factory();
    if (fbp_port_register(self->logp, &port_config)) {
        fbp_stack_finalize(self);
        return NULL;
    }

    return self;
}

int32_t fbp_stack_finalize(struct fbp_stack_s * self) {
    if (self) {
        if (self->framer) {
            fbp_framer_finalize(self->framer);
            self->framer = NULL;
        }
        if (self->dl) {
            fbp_dl_finalize(self->dl);
            self->dl = NULL;
        }
        if (self->transport) {
            fbp_transport_finalize(self->transport);
            self->transport = NULL;
        }
        if (self->port0) {
            fbp_port0_finalize(self->port0);
            self->port0 = NULL;
        }
        if (self->pubsub_port) {
            fbp_pubsubp_finalize(self->pubsub_port);
            self->pubsub_port = NULL;
        }
        if (self->logp) {
            self->logp->finalize(self->logp);
            self->logp = NULL;
        }
        fbp_free(self);
    }
    return 0;
}

void fbp_stack_mutex_set(struct fbp_stack_s * self, fbp_os_mutex_t mutex) {
    fbp_dl_register_mutex(self->dl, mutex);
}
