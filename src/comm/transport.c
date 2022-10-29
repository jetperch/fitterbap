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

#include "fitterbap/comm/transport.h"
#include "fitterbap/ec.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"


struct port_s {
    void *user_data;
    const char * meta;
    fbp_transport_event_fn event_fn;
    fbp_transport_recv_fn recv_fn;
};

/// The transport instance.
struct fbp_transport_s {
    fbp_transport_ll_send send_fn;
    void * send_user_data;
    /// The defined ports.
    struct port_s ports[FBP_TRANSPORT_PORT_MAX];
    struct port_s port_default;
    enum fbp_dl_event_e last_state_event;   // to properly initialize new port registrations
};

void fbp_transport_on_event_cbk(struct fbp_transport_s * self, enum fbp_dl_event_e event) {
    switch (event) {
        case FBP_DL_EV_CONNECTED:       // intentional fall-through
        case FBP_DL_EV_DISCONNECTED:    // intentional fall-through
        case FBP_DL_EV_TRANSPORT_CONNECTED:
        case FBP_DL_EV_APP_CONNECTED:
            self->last_state_event = event;
            break;
        default:
            break;
    }
    for (uint32_t i = 0; i < FBP_TRANSPORT_PORT_MAX; ++i) {
        if (self->ports[i].event_fn) {
            self->ports[i].event_fn(self->ports[i].user_data, event);
        }
    }
    if (self->port_default.event_fn) {
        self->port_default.event_fn(self->port_default.user_data, event);
    }
}

void fbp_transport_on_recv_cbk(struct fbp_transport_s * self, uint16_t metadata,
                    uint8_t *msg, uint32_t msg_size) {
    uint8_t port_id = metadata & FBP_TRANSPORT_PORT_MAX;
    enum fbp_transport_seq_e seq = (enum fbp_transport_seq_e) ((metadata >> 6) & 3);
    uint8_t port_data = (uint8_t) (metadata >> 8);
    if (self->ports[port_id].recv_fn) {
        self->ports[port_id].recv_fn(self->ports[port_id].user_data, port_id, seq, port_data, msg, msg_size);
    } else if (self->port_default.recv_fn) {
        self->port_default.recv_fn(self->port_default.user_data, port_id, seq, port_data, msg, msg_size);
    } else {
        // no registered handlers, drop silently
    }
}

struct fbp_transport_s * fbp_transport_initialize(fbp_transport_ll_send send_fn, void * send_user_data) {
    struct fbp_transport_s * t = fbp_alloc_clr(sizeof(struct fbp_transport_s));
    FBP_ASSERT_ALLOC(t);
    t->last_state_event = FBP_DL_EV_DISCONNECTED;
    t->send_fn = send_fn;
    t->send_user_data = send_user_data;
    return t;
}

void fbp_transport_finalize(struct fbp_transport_s * self) {
    if (self) {
        fbp_free(self);
    }
}

int32_t fbp_transport_port_register(struct fbp_transport_s * self,
                                     uint8_t port_id,
                                     const char * meta,
                                     fbp_transport_event_fn event_fn,
                                     fbp_transport_recv_fn recv_fn,
                                     void * user_data) {
    if (port_id > FBP_TRANSPORT_PORT_MAX) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    self->ports[port_id].event_fn = NULL;
    self->ports[port_id].recv_fn = NULL;
    self->ports[port_id].meta = meta;
    self->ports[port_id].user_data = user_data;
    self->ports[port_id].event_fn = event_fn;
    self->ports[port_id].recv_fn = recv_fn;
    if (event_fn) {
        event_fn(user_data, self->last_state_event);
    }
    return 0;
}

int32_t fbp_transport_port_register_default(
        struct fbp_transport_s * self,
        fbp_transport_event_fn event_fn,
        fbp_transport_recv_fn recv_fn,
        void * user_data) {
    self->port_default.event_fn = NULL;
    self->port_default.recv_fn = NULL;
    self->port_default.user_data = user_data;
    self->port_default.event_fn = event_fn;
    self->port_default.recv_fn = recv_fn;
    if (event_fn) {
        event_fn(user_data, self->last_state_event);
    }
    return 0;
}

int32_t fbp_transport_send(struct fbp_transport_s * self,
                           uint8_t port_id,
                           enum fbp_transport_seq_e seq,
                           uint8_t port_data,
                           uint8_t const *msg, uint32_t msg_size) {
    if (port_id > FBP_TRANSPORT_PORT_MAX) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    uint16_t metadata = ((seq & 0x3) << 6)
        | (port_id & FBP_TRANSPORT_PORT_MAX)
        | (((uint16_t) port_data) << 8);

    return self->send_fn(self->send_user_data, metadata, msg, msg_size);
}

const char * fbp_transport_meta_get(struct fbp_transport_s * self, uint8_t port_id) {
    if (port_id > FBP_TRANSPORT_PORT_MAX) {
        return NULL;
    }
    return self->ports[port_id].meta;
}

void fbp_transport_event_inject(struct fbp_transport_s * self, enum fbp_dl_event_e event) {
    switch (event) {
        case FBP_DL_EV_TRANSPORT_CONNECTED: break;
        case FBP_DL_EV_APP_CONNECTED: break;
        default:
            return;
    }
    fbp_transport_on_event_cbk(self, event);
}
