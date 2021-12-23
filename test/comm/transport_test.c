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
#include "fitterbap/comm/transport.h"
#include "fitterbap/platform.h"


uint8_t DATA1[] = {1, 2, 3, 4, 5, 6, 7, 8};


struct fbp_dl_s {
    struct fbp_transport_s * t;
};

static int32_t ll_send(void * user_data, uint16_t metadata,
                     uint8_t const *msg, uint32_t msg_size) {
    (void) user_data;
    check_expected(metadata);
    check_expected(msg_size);
    check_expected_ptr(msg);
    return 0;
}

#define expect_send(_metadata, _msg_data, _msg_size, _timeout_ms)    \
    expect_value(ll_send, metadata, _metadata);    \
    expect_value(ll_send, msg_size, _msg_size );   \
    expect_memory(ll_send, msg, _msg_data, _msg_size);

static int setup(void ** state) {
    (void) state;
    struct fbp_dl_s * self = fbp_alloc_clr(sizeof(struct fbp_dl_s));
    self->t = fbp_transport_initialize(ll_send, self);
    assert_non_null(self->t);
    fbp_transport_on_event_cbk(self->t, FBP_DL_EV_CONNECTED);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) *state;
    fbp_transport_finalize(self->t);
    fbp_free(self);
    return 0;
}

static void test_send(void ** state) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) *state;
    expect_send(0x12C0, DATA1, sizeof(DATA1), 0);
    assert_int_equal(0, fbp_transport_send(self->t, 0, FBP_TRANSPORT_SEQ_SINGLE, 0x12, DATA1, sizeof(DATA1)));
    expect_send(0x34df, DATA1, sizeof(DATA1), 100);
    assert_int_equal(0, fbp_transport_send(self->t, 0x1f, FBP_TRANSPORT_SEQ_SINGLE, 0x34, DATA1, sizeof(DATA1)));
    assert_int_not_equal(0, fbp_transport_send(self->t, FBP_TRANSPORT_PORT_MAX + 1, FBP_TRANSPORT_SEQ_SINGLE, 0, DATA1, sizeof(DATA1)));
}

static void on_event(void *user_data, enum fbp_dl_event_e event) {
    (void) user_data;
    check_expected(event);
}

#define expect_event(_event)    \
    expect_value(on_event, event, _event);

static void on_recv(void *user_data, uint8_t port_id,
                    enum fbp_transport_seq_e seq, uint8_t port_data,
                    uint8_t *msg, uint32_t msg_size) {
    (void) user_data;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
}

#define expect_recv(_port_id, _seq, _port_data, _msg_data, _msg_size) \
    expect_value(on_recv, port_id, _port_id);                         \
    expect_value(on_recv, seq, _seq);                                 \
    expect_value(on_recv, port_data, _port_data);                     \
    expect_value(on_recv, msg_size, _msg_size);                       \
    expect_memory(on_recv, msg, _msg_data, _msg_size)

static void test_event(void ** state) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) *state;
    expect_event(FBP_DL_EV_CONNECTED);
    assert_int_equal(0, fbp_transport_port_register(self->t, 1, NULL, on_event, on_recv, self));
    expect_event(FBP_DL_EV_RESET_REQUEST);
    fbp_transport_on_event_cbk(self->t, FBP_DL_EV_RESET_REQUEST);
}

static void test_event_when_not_connected(void ** state) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) *state;
    fbp_transport_on_event_cbk(self->t, FBP_DL_EV_DISCONNECTED);
    expect_event(FBP_DL_EV_DISCONNECTED);
    assert_int_equal(0, fbp_transport_port_register(self->t, 1, NULL, on_event, on_recv, self));
}

static uint16_t metadata_pack(uint8_t frame_id, enum fbp_transport_seq_e seq, uint8_t port_data) {
    return frame_id | (seq << 6) | (((uint16_t) port_data) << 8);
}

#define MPACK(frame_id, seq, port_data)  metadata_pack(frame_id, FBP_TRANSPORT_SEQ_##seq, port_data)

static void test_recv(void ** state) {
    struct fbp_dl_s * self = (struct fbp_dl_s *) *state;
    expect_event(FBP_DL_EV_CONNECTED);
    assert_int_equal(0, fbp_transport_port_register(self->t, 1, NULL, on_event, on_recv, self));

    expect_recv(1, FBP_TRANSPORT_SEQ_SINGLE, 0x12, DATA1, sizeof(DATA1));
    fbp_transport_on_recv_cbk(self->t, MPACK(1, SINGLE, 0x12), DATA1, sizeof(DATA1));

    expect_recv(1, FBP_TRANSPORT_SEQ_START, 0xAB, DATA1, sizeof(DATA1));
    fbp_transport_on_recv_cbk(self->t, MPACK(1, START, 0xAB), DATA1, sizeof(DATA1));

    expect_recv(1, FBP_TRANSPORT_SEQ_MIDDLE, 0, DATA1, sizeof(DATA1));
    fbp_transport_on_recv_cbk(self->t, MPACK(1, MIDDLE, 0), DATA1, sizeof(DATA1));

    expect_recv(1, FBP_TRANSPORT_SEQ_STOP, 0, DATA1, sizeof(DATA1));
    fbp_transport_on_recv_cbk(self->t, MPACK(1, STOP, 0), DATA1, sizeof(DATA1));

    // no registered handler, will be dropped
    fbp_transport_on_recv_cbk(self->t, 0x7, DATA1, sizeof(DATA1));
}

static void on_event2(void *user_data, enum fbp_dl_event_e event) {
    (void) user_data;
    check_expected(event);
}

#define expect_event2(_event)    \
    expect_value(on_event2, event, _event);

static void on_recv2(void *user_data, uint8_t port_id,
                     enum fbp_transport_seq_e seq, uint8_t port_data,
                     uint8_t *msg, uint32_t msg_size) {
    (void) user_data;
    check_expected(port_id);
    (void) seq;
    (void) port_data;
    (void) msg;
    (void) msg_size;
}

#define expect_recv2(_port_id)    \
    expect_value(on_recv2, port_id, _port_id);

static void test_default(void ** state) {
    struct fbp_dl_s *self = (struct fbp_dl_s *) *state;
    expect_event2(FBP_DL_EV_CONNECTED);
    assert_int_equal(0, fbp_transport_port_register(self->t, 1, NULL, on_event2, on_recv2, self));
    expect_event(FBP_DL_EV_CONNECTED);
    assert_int_equal(0, fbp_transport_port_register_default(self->t, on_event, on_recv, self));

    expect_recv2(1);
    fbp_transport_on_recv_cbk(self->t, 0x12C1, DATA1, sizeof(DATA1));

    expect_recv(2, FBP_TRANSPORT_SEQ_STOP, 0, DATA1, sizeof(DATA1));
    fbp_transport_on_recv_cbk(self->t, MPACK(2, STOP, 0), DATA1, sizeof(DATA1));

    expect_event2(FBP_DL_EV_RESET_REQUEST);
    expect_event(FBP_DL_EV_RESET_REQUEST);
    fbp_transport_on_event_cbk(self->t, FBP_DL_EV_RESET_REQUEST);
}

static void test_event_inject(void ** state) {
    struct fbp_dl_s *self = (struct fbp_dl_s *) *state;

    expect_event(FBP_DL_EV_CONNECTED);
    assert_int_equal(0, fbp_transport_port_register(self->t, 1, NULL, on_event, on_recv, self));

    expect_event(FBP_DL_EV_TRANSPORT_CONNECTED);
    fbp_transport_event_inject(self->t, FBP_DL_EV_TRANSPORT_CONNECTED);

    expect_event(FBP_DL_EV_APP_CONNECTED);
    fbp_transport_event_inject(self->t, FBP_DL_EV_APP_CONNECTED);

    // ignored
    fbp_transport_event_inject(self->t, FBP_DL_EV_DISCONNECTED);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_send, setup, teardown),
            cmocka_unit_test_setup_teardown(test_event, setup, teardown),
            cmocka_unit_test_setup_teardown(test_event_when_not_connected, setup, teardown),
            cmocka_unit_test_setup_teardown(test_recv, setup, teardown),
            cmocka_unit_test_setup_teardown(test_default, setup, teardown),
            cmocka_unit_test_setup_teardown(test_event_inject, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
