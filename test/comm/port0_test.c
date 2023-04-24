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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "fitterbap/comm/port0.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/comm/transport.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"


#define EVENT_COUNT_MAX (8)


struct fbp_dl_s {
    int32_t hello;
};

struct ev_s {
    int32_t event_id;
    int64_t timestamp;
    fbp_evm_callback cbk_fn;
    void * cbk_user_data;
};

struct fbp_transport_s {
    struct fbp_port0_s * p1;
    struct fbp_port0_s * p2;
    struct fbp_pubsub_s * pubsub1;
    struct fbp_pubsub_s * pubsub2;
    struct fbp_dl_s dl1;
    struct fbp_dl_s dl2;
    struct fbp_evm_api_s evm;
    int64_t timestamp;
    uint64_t counter;
    struct ev_s events[EVENT_COUNT_MAX];
};

struct fbp_transport_s * instance_;

#define META_PORT0 "{\"type\":\"oam\", \"name\": \"oam\"}"
#define META_PORT1 "{\"type\":\"pubsub\", \"name\": \"pubsub\"}"

#define META_MSG_PORT0 "\x20" META_PORT0
#define META_MSG_PORT1 "\x21" META_PORT1
#define TX_WINDOW_SIZE 16
#define RX_WINDOW_SIZE 12

uint32_t fbp_time_counter_frequency_() {
    return 1000;
}

uint64_t fbp_time_counter_u64_() {
    return instance_->counter;
}

uint32_t fbp_time_counter_u32_() {
    return (uint32_t) instance_->counter;
}

void fbp_ts_update(struct fbp_ts_s * self, uint64_t src_tx, int64_t tgt_rx, int64_t tgt_tx, uint64_t src_rx) {
    (void) self;
    (void) src_tx;
    (void) tgt_rx;
    (void) tgt_tx;
    (void) src_rx;
}

void fbp_dl_reset_tx_from_event(struct fbp_dl_s * dl_ptr) {
    if (!instance_->p2) {
        intptr_t dl = (intptr_t) dl_ptr;
        check_expected(dl);
        return;
    }
    struct fbp_port0_s * psrc = instance_->p1;
    struct fbp_port0_s * pdst = instance_->p2;
    if (dl_ptr == &instance_->dl2) {
        psrc = instance_->p2;
        pdst = instance_->p1;
    }
    fbp_port0_on_event_cbk(pdst, FBP_DL_EV_RESET_REQUEST);
    fbp_port0_on_event_cbk(psrc, FBP_DL_EV_CONNECTED);
}

int32_t fbp_dl_status_get(
        struct fbp_dl_s * self,
        struct fbp_dl_status_s * status) {
    (void) self;
    memset(status, 0, sizeof(*status));
    return 0;
}

uint32_t fbp_dl_tx_window_max_get(struct fbp_dl_s * self) {
    (void) self;
    return TX_WINDOW_SIZE;
}

void fbp_dl_tx_window_set(struct fbp_dl_s * self, uint32_t tx_window_size) {
    intptr_t dl = (intptr_t) self;
    check_expected(dl);
    check_expected(tx_window_size);
}

#define expect_tx_window_set(dl_, tx_window_size_) \
    expect_value(fbp_dl_tx_window_set, dl, (intptr_t) dl_);   \
    expect_value(fbp_dl_tx_window_set, tx_window_size, tx_window_size_);

uint32_t fbp_dl_rx_window_get(struct fbp_dl_s * self) {
    (void) self;
    return RX_WINDOW_SIZE;
}

void fbp_transport_event_inject(struct fbp_transport_s * self, enum fbp_dl_event_e event) {
    (void) self;
    check_expected(event);
}

#define expect_dl_event_inject(dl_, event_) \
    expect_value(fbp_transport_event_inject, event, event_);

int64_t fbp_time_utc_() {
    return instance_->timestamp;
}

const char * fbp_transport_meta_get(struct fbp_transport_s * self, uint8_t port_id) {
    (void) self;
    switch (port_id) {
        case 0: return META_PORT0;
        case 1: return META_PORT1;
        default: return NULL;
    }
}

void fbp_os_mutex_lock_(fbp_os_mutex_t mutex) {
    (void) mutex;
}

void fbp_os_mutex_unlock_(fbp_os_mutex_t mutex) {
    (void) mutex;
}

static int32_t ll_send(struct fbp_transport_s * t,
                       uint8_t port_id,
                       enum fbp_transport_seq_e seq,
                       uint8_t port_data,
                       uint8_t const *msg, uint32_t msg_size) {
    (void) t;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
    return 0;
}

#define expect_send(_port_id, _seq, _port_data, _msg_data, _msg_size)  \
    expect_value(ll_send, port_id, _port_id);                          \
    expect_value(ll_send, seq, _seq);                                  \
    expect_value(ll_send, port_data, _port_data);                      \
    expect_value(ll_send, msg_size, _msg_size );                       \
    expect_memory(ll_send, msg, _msg_data, _msg_size)

#define expect_send_ignore_msg(_port_id, _seq, _port_data)             \
    expect_value(ll_send, port_id, _port_id);                          \
    expect_value(ll_send, seq, _seq);                                  \
    expect_value(ll_send, port_data, _port_data);                      \
    expect_any(ll_send, msg_size);                                     \
    expect_any(ll_send, msg)


int64_t evm_timestamp_fn(struct fbp_evm_s * evm) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) evm;
    return self->timestamp;
}

int32_t evm_schedule_fn(struct fbp_evm_s * evm, int64_t timestamp,
                        fbp_evm_callback cbk_fn, void * cbk_user_data) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) evm;
    for (int i = 0; i < EVENT_COUNT_MAX; ++i) {
        if (!self->events[i].cbk_fn) {
            self->events[i].timestamp = timestamp;
            self->events[i].cbk_fn = cbk_fn;
            self->events[i].cbk_user_data = cbk_user_data;
            return i + 1;
        };
    }
    return 0;
}

int32_t evm_cancel_fn(struct fbp_evm_s * evm, int32_t event_id) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) evm;
    if ((event_id <= 0) || (event_id > EVENT_COUNT_MAX)) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    int32_t idx = event_id - 1;
    assert_true(self->events[idx].cbk_fn != NULL);
    self->events[idx].cbk_fn = 0;
    return 0;
}

int32_t evm_count(struct fbp_transport_s * self) {
    int32_t count = 0;
    for (int i = 0; i < EVENT_COUNT_MAX; ++i) {
        if (self->events[i].cbk_fn) {
            ++count;
        }
    }
    return count;
}

int32_t evm_next_idx(struct fbp_transport_s * self) {
    int64_t next_event_time = INT64_MAX;
    int32_t next_event_idx = -1;
    for (int i = 0; i < EVENT_COUNT_MAX; ++i) {
        if (self->events[i].cbk_fn && (self->events[i].timestamp < next_event_time)) {
            next_event_time = self->events[i].timestamp;
            next_event_idx = i;
        }
    }
    assert_true(next_event_idx >= 0);
    return next_event_idx;
}

void evm_process_next(struct fbp_transport_s * self) {
    int32_t idx = evm_next_idx(self);
    self->timestamp = self->events[idx].timestamp;
    fbp_evm_callback cbk = self->events[idx].cbk_fn;
    self->events[idx].cbk_fn = 0;
    cbk(self->events[idx].cbk_user_data, idx + 1);
}

static void setup_evm(struct fbp_transport_s * self) {
    self->evm.evm = (struct fbp_evm_s *) self;
    for (int i = 0; i < EVENT_COUNT_MAX; ++i) {
        self->events[i].event_id = i + 1;
    }
    self->evm.timestamp = evm_timestamp_fn;
    self->evm.schedule = evm_schedule_fn;
    self->evm.cancel = evm_cancel_fn;
}

static int setup(void ** state) {
    (void) state;
    struct fbp_transport_s * self = fbp_alloc_clr(sizeof(struct fbp_transport_s));
    instance_ = self;
    setup_evm(self);
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) *state;
    if (self->p1) {
        fbp_port0_finalize(self->p1);
    }
    if (self->pubsub1) {
        fbp_pubsub_finalize(self->pubsub1);
    }
    fbp_free(self);
    return 0;
}

#define INITIALIZE(mode_) \
    struct fbp_transport_s * self = (struct fbp_transport_s *) *state; \
    struct fbp_pubsub_s * pubsub = fbp_pubsub_initialize("h", 1024);   \
    assert_non_null(pubsub);                                           \
    struct fbp_port0_s * p = fbp_port0_initialize(FBP_PORT0_MODE_##mode_, &self->dl1, &self->evm, self, ll_send, pubsub, "h/c0/", NULL); \
    assert_non_null(p); \
    self->p1 = p

#define FINALIZE() \
    self->p1 = NULL; \
    fbp_port0_finalize(p); \
    fbp_pubsub_finalize(pubsub);

#define REQ(op) (0x00 | ((FBP_PORT0_OP_##op) & 0x07))
#define RSP(op) (0x80 | ((FBP_PORT0_OP_##op) & 0x07))

static void server_timestamp(struct fbp_transport_s * self) {
    // timestamp, receive request and respond
    int64_t timesync_req[5] = {0, 0, 0, 0, 0};
    int64_t timesync_rsp[5] = {0, 0, 0, 0, 0};
    self->timestamp += FBP_TIME_MILLISECOND * 10;
    timesync_req[1] = self->timestamp;
    timesync_rsp[0] = timesync_req[0];
    timesync_rsp[1] = timesync_req[1];
    self->timestamp += FBP_TIME_MILLISECOND;
    timesync_rsp[1] = timesync_req[1];
    timesync_rsp[2] = self->timestamp;
    timesync_rsp[3] = self->timestamp;
    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, RSP(TIMESYNC), timesync_rsp, sizeof(timesync_rsp));
    fbp_port0_on_recv_cbk(self->p1, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(TIMESYNC), (uint8_t *) timesync_req, sizeof(timesync_req));
}

#define echo_one(self, idx)  {                                                                      \
    static uint8_t echo[] = {0, 1, 2, 3, 4, 5, 6, 7};                                               \
    echo[0] = idx;                                                                                  \
    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, RSP(ECHO), echo, sizeof(echo));                        \
    fbp_port0_on_recv_cbk(self->p1, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(ECHO), echo, sizeof(echo));     \
}

static void test_server_connect(void ** state) {
    INITIALIZE(SERVER);

    // disconnected -> negotiate
    fbp_port0_on_event_cbk(p, FBP_DL_EV_CONNECTED);

    // process tick to initiate negotiation req
    uint32_t negotiate_payload[4] = {FBP_DL_VERSION, 0, TX_WINDOW_SIZE, RX_WINDOW_SIZE};
    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, REQ(NEGOTIATE), negotiate_payload, sizeof(negotiate_payload));
    evm_process_next(self);

    // negotiate -> meta
    negotiate_payload[2] = RX_WINDOW_SIZE;
    expect_tx_window_set(&self->dl1, RX_WINDOW_SIZE);
    fbp_port0_on_recv_cbk(p, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(NEGOTIATE), (uint8_t *) negotiate_payload, sizeof(negotiate_payload));

    // client is in timestamp states, should handle these in all server states
    server_timestamp(self);
    server_timestamp(self);

    for (int i = 0; i < 32; ++i) {
        char meta[] = " {\"type\":\"oam\", \"name\": \"oam\"}";
        meta[0] = i + 32;
        if (i == 31) {
            expect_dl_event_inject(&self->dl1, FBP_DL_EV_TRANSPORT_CONNECTED);
        }
        fbp_port0_on_recv_cbk(p, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(META), (uint8_t *) meta, sizeof(meta));
    }

    server_timestamp(self);
    echo_one(self, 1);
    echo_one(self, 2);
    echo_one(self, 3);

    assert_int_equal(0, evm_count(self));

    FINALIZE();
}

static void client_timestamp(struct fbp_transport_s * self) {
    // timestamp, receive request and respond
    int64_t timesync[5] = {0, 0, 0, 0, 0};
    self->counter = 1234;
    timesync[1] = self->counter;
    assert_int_equal(self->timestamp, fbp_time_utc());

    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, REQ(TIMESYNC), timesync, sizeof(timesync));
    evm_process_next(self);

    self->timestamp += FBP_TIME_MILLISECOND;
    timesync[2] = self->timestamp;
    timesync[3] = self->timestamp;
    fbp_port0_on_recv_cbk(self->p1, 0, FBP_TRANSPORT_SEQ_SINGLE, RSP(TIMESYNC), (uint8_t *) timesync, sizeof(timesync));
}

static void test_client_connect(void ** state) {
    INITIALIZE(CLIENT);

    // stay in disconnected on rx_reset
    self->timestamp += FBP_TIME_MILLISECOND * 10;

    // disconnected -> negotiate
    fbp_port0_on_event_cbk(p, FBP_DL_EV_CONNECTED);

    // negotiate -> timesync
    uint32_t negotiate_req[4] = {FBP_DL_VERSION, 0, TX_WINDOW_SIZE, RX_WINDOW_SIZE};
    uint32_t negotiate_rsq[4] = {FBP_DL_VERSION, 0, RX_WINDOW_SIZE, RX_WINDOW_SIZE};
    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, RSP(NEGOTIATE), negotiate_rsq, sizeof(negotiate_rsq));

    // negotiate -> timestamp
    expect_tx_window_set(&self->dl1, RX_WINDOW_SIZE);
    fbp_port0_on_recv_cbk(p, 0, FBP_TRANSPORT_SEQ_SINGLE, REQ(NEGOTIATE), (uint8_t *) negotiate_req, sizeof(negotiate_req));

    client_timestamp(self);
    client_timestamp(self);

    // meta
    for (int i = 0; i < 32; ++i) {
        expect_send_ignore_msg(0, FBP_TRANSPORT_SEQ_SINGLE, RSP(META));
    }
    expect_dl_event_inject(&self->dl1, FBP_DL_EV_TRANSPORT_CONNECTED);
    evm_process_next(self);

    client_timestamp(self);

    echo_one(self, 1);
    echo_one(self, 2);
    echo_one(self, 3);

    assert_int_equal(1, evm_count(self));
    client_timestamp(self);
    client_timestamp(self);
    client_timestamp(self);

    FINALIZE();
}

static void test_server_timeout_in_negotiate(void ** state) {
    INITIALIZE(SERVER);

    // disconnected -> negotiate
    self->timestamp += FBP_TIME_MILLISECOND * 10;
    fbp_port0_on_event_cbk(p, FBP_DL_EV_CONNECTED);

    // await_client -> negotiate
    uint32_t negotiate_payload[4] = {FBP_DL_VERSION, 0, TX_WINDOW_SIZE, RX_WINDOW_SIZE};
    expect_send(0, FBP_TRANSPORT_SEQ_SINGLE, REQ(NEGOTIATE), negotiate_payload, sizeof(negotiate_payload));
    evm_process_next(self);

    // no negotiate response causes timeout: negotiate -> disconnect
    expect_value(fbp_dl_reset_tx_from_event, dl, (intptr_t) &self->dl1);
    evm_process_next(self);

    FINALIZE();
}

static int32_t send_p1_to_p2(struct fbp_transport_s * t,
                             uint8_t port_id, enum fbp_transport_seq_e seq, uint8_t port_data,
                             uint8_t const *msg, uint32_t msg_size) {
    fbp_port0_on_recv_cbk(t->p2, port_id, seq, port_data, (uint8_t *) msg, msg_size);
    return 0;
}

static int32_t send_p2_to_p1(struct fbp_transport_s * t,
                             uint8_t port_id, enum fbp_transport_seq_e seq, uint8_t port_data,
                             uint8_t const *msg, uint32_t msg_size) {
    fbp_port0_on_recv_cbk(t->p1, port_id, seq, port_data, (uint8_t *) msg, msg_size);
    return 0;
}

static void setup_dual(struct fbp_transport_s * self) {
    self->pubsub1 = fbp_pubsub_initialize("h", 1024);
    assert_non_null(self->pubsub1);
    self->pubsub2 = fbp_pubsub_initialize("d", 1024);
    assert_non_null(self->pubsub2);

    self->p1 = fbp_port0_initialize(FBP_PORT0_MODE_SERVER, &self->dl1, &self->evm, self, send_p1_to_p2, self->pubsub1, "h/c0/", NULL); \
    assert_non_null(self->p1);

    self->p2 = fbp_port0_initialize(FBP_PORT0_MODE_CLIENT, &self->dl2, &self->evm, self, send_p2_to_p1, self->pubsub2, "d/c0/", NULL); \
    assert_non_null(self->p2);
}

static void teardown_dual(struct fbp_transport_s * self) {
    fbp_port0_finalize(self->p1);
    fbp_port0_finalize(self->p2);
    self->p1 = NULL;
    self->p2 = NULL;
    fbp_pubsub_finalize(self->pubsub1);
    fbp_pubsub_finalize(self->pubsub2);
    self->pubsub1 = NULL;
    self->pubsub2 = NULL;
}

static uint8_t on_publish_u32(void * user_data, const char * topic, const struct fbp_union_s * value) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) user_data;
    (void) self;
    check_expected_ptr(topic);
    assert_int_equal(FBP_UNION_U32, value->type);
    uint32_t state = value->value.u32;
    check_expected(state);
    return 0;
}

#define expect_publish_u32(topic_, value_) \
    expect_string(on_publish_u32, topic, topic_); \
    expect_value(on_publish_u32, state, value_);

static void test_connect(void ** state) {
    struct fbp_transport_s * self = (struct fbp_transport_s *) *state;
    setup_dual(self);
    fbp_pubsub_subscribe(self->pubsub1, "h/c0/0/state", FBP_PUBSUB_SFLAG_PUB, on_publish_u32, self);
    fbp_pubsub_subscribe(self->pubsub2, "d/c0/0/state", FBP_PUBSUB_SFLAG_PUB, on_publish_u32, self);

    fbp_port0_on_event_cbk(self->p1, FBP_DL_EV_CONNECTED);
    fbp_port0_on_event_cbk(self->p2, FBP_DL_EV_CONNECTED);

    self->timestamp += FBP_TIME_MILLISECOND * 10;

    expect_tx_window_set(&self->dl2, RX_WINDOW_SIZE);
    expect_tx_window_set(&self->dl1, RX_WINDOW_SIZE);

    expect_dl_event_inject(&self->dl1, FBP_DL_EV_TRANSPORT_CONNECTED);
    expect_dl_event_inject(&self->dl2, FBP_DL_EV_TRANSPORT_CONNECTED);

    evm_process_next(self);
    evm_process_next(self);
    evm_process_next(self);
    evm_process_next(self);
    expect_publish_u32("h/c0/0/state", 1);
    expect_publish_u32("d/c0/0/state", 1);
    evm_process_next(self);

    teardown_dual(self);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_server_connect, setup, teardown),
            cmocka_unit_test_setup_teardown(test_client_connect, setup, teardown),
            cmocka_unit_test_setup_teardown(test_server_timeout_in_negotiate, setup, teardown),
            cmocka_unit_test_setup_teardown(test_connect, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
