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

#include "../hal_test_impl.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "fitterbap/comm/data_link.h"
#include "fitterbap/collections/ring_buffer_u64.h"
#include "fitterbap/time.h"
#include <stdio.h>

#define SEND_BUFFER_SIZE (1 << 13)
static uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
// print(', '.join(['0x%02x' % x for x in range(256)]))
static uint8_t PAYLOAD_MAX[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
        0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
        0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
        0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

struct test_s;
struct fbp_dl_s * dl_ = 0;
uint32_t send_available_ = 0;
int64_t now_ = 0;
static struct fbp_evm_s * evm_;

static int64_t ll_time_get(struct fbp_evm_s * evm) {
    (void) evm;
    return now_;
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    if (buffer[2] & 0xE0) {
        while (buffer_size) {
            uint8_t event = (buffer[2] >> 5) & 0x7;
            uint16_t frame_id = (((uint16_t) (buffer[2] & 0x7)) << 8) | buffer[3];
            check_expected(event);
            check_expected(frame_id);
            buffer += FBP_FRAMER_LINK_SIZE;
            buffer_size -= FBP_FRAMER_LINK_SIZE;
        }
    } else {
        check_expected(buffer_size);
        check_expected_ptr(buffer);
    }
}

static uint32_t ll_send_available(void * user_data) {
    (void) user_data;
    return send_available_; // todo
}

static void on_event(void *user_data, enum fbp_dl_event_e event) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(event);
}

#define expect_event(expected_event)  expect_value(on_event, event, (expected_event))

static void on_recv(void *user_data, uint32_t metadata, uint8_t *msg_buffer, uint32_t msg_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(metadata);
    check_expected(msg_size);
    check_expected_ptr(msg_buffer);
}

static void expect_recv(uint32_t metadata, uint8_t *msg_buffer, uint32_t msg_size) {
    expect_value(on_recv, metadata, metadata);
    expect_value(on_recv, msg_size, msg_size);
    expect_memory(on_recv, msg_buffer, msg_buffer, msg_size);
}

static int setup(void ** state) {
    struct test_s *self = NULL;
    //self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    struct fbp_evm_api_s evm_api;
    fbp_evm_api_get(evm_, &evm_api);
    evm_api.timestamp = ll_time_get;

    struct fbp_dl_config_s config = {
        .tx_window_size = 64,
        .tx_buffer_size = (1 << 13),
        .rx_window_size = 64,
        .tx_timeout = 10 * FBP_TIME_MILLISECOND,
        .tx_link_size = 64,
    };

    struct fbp_dl_ll_s ll = {
        .user_data = self,
        .send = ll_send,
        .send_available = ll_send_available,
    };

    struct fbp_dl_api_s ul = {
        .user_data = self,
        .event_fn = on_event,
        .recv_fn = on_recv,
    };

    now_ = 0;
    send_available_ = FBP_FRAMER_MAX_SIZE;
    dl_ = fbp_dl_initialize(&config, &evm_api, &ll);
    assert_non_null(dl_);
    fbp_dl_register_upper_layer(dl_, &ul);

    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_dl_finalize(dl_);
    test_free(self);

    fbp_evm_process(evm_, FBP_TIME_YEAR * 100);
    fbp_evm_process(evm_, 0);

    return 0;
}

static void test_initial_state(void ** state) {
    (void) state;
    struct fbp_dl_status_s status;

    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(1, status.version);
    assert_int_equal(0, status.rx.msg_bytes);
    assert_int_equal(0, status.rx.data_frames);
    assert_int_equal(0, status.rx_framer.total_bytes);
    assert_int_equal(0, status.rx_framer.ignored_bytes);
    assert_int_equal(0, status.rx_framer.resync);
    assert_int_equal(0, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);
}

static void expect_send_data(struct test_s *self,
                             uint16_t frame_id, uint16_t metadata,
                             uint8_t *msg_buffer, uint32_t msg_size) {
    (void) self;
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(0, fbp_framer_construct_data(b, frame_id, metadata, msg_buffer, msg_size));
    uint16_t frame_sz = msg_size + FBP_FRAMER_OVERHEAD_SIZE;
    expect_value(ll_send, buffer_size, frame_sz);
    expect_memory(ll_send, buffer, b, frame_sz);
}

static void send_and_expect(struct test_s *self,
                 uint16_t frame_id, uint16_t metadata,
                 uint8_t *msg_buffer, uint32_t msg_size) {
    expect_send_data(self, frame_id, metadata, msg_buffer, msg_size);
    assert_int_equal(0, fbp_dl_send(dl_, metadata, msg_buffer, msg_size, 0));
}

static void expect_send_link(struct test_s *self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    (void) self;
    uint64_t u64 = 0;
    assert_int_equal(0, fbp_framer_construct_link((uint8_t *) &u64, frame_type, frame_id));
    expect_value(ll_send, event, frame_type);
    expect_value(ll_send, frame_id, frame_id);
}

static void recv_link(struct test_s *self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    (void) self;
    uint8_t b[FBP_FRAMER_LINK_SIZE];
    assert_int_equal(0, fbp_framer_construct_link(b, frame_type, frame_id));
    fbp_dl_ll_recv(dl_, b, sizeof(b));
}

static void recv_data(struct test_s *self, uint16_t frame_id, uint16_t metadata,
                      uint8_t *msg_buffer, uint32_t msg_size) {
    (void) self;
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(0, fbp_framer_construct_data(b, frame_id, metadata, msg_buffer, msg_size));
    uint16_t frame_sz = msg_size + FBP_FRAMER_OVERHEAD_SIZE;
    fbp_dl_ll_recv(dl_, b, frame_sz);
}

static void connect(struct test_s *self) {
    (void) self;
    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    fbp_dl_process(dl_);
    expect_event(FBP_DL_EV_TX_CONNECTED);
    recv_link(self, FBP_FRAMER_FT_RESET, 1);
    fbp_dl_status_clear(dl_);
}

void on_send_fn(void * user_data) {
    (void) user_data;
    int value = 0;
    check_expected(value);
}

static void test_on_send_cbk(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    connect(self);
    fbp_dl_register_on_send(dl_, on_send_fn, self);
    expect_value(on_send_fn, value, 0);
    assert_int_equal(0, fbp_dl_send(dl_, 1, PAYLOAD1, sizeof(PAYLOAD1), 0));
}

static void test_send_data_with_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;

    connect(self);
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    fbp_dl_process(dl_);

    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
}

static void test_send_nack_resend_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;

    connect(self);
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    fbp_dl_process(dl_);

    recv_link(self, FBP_FRAMER_FT_NACK_FRAMING_ERROR, 0);
    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(0, status.tx.data_frames);

    expect_send_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));  // due to nack
    fbp_dl_process(dl_);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(1, status.tx.data_frames);
}

static void test_send_data_timeout_then_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;

    connect(self);
    now_ = 5 * FBP_TIME_MILLISECOND;
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    fbp_dl_process(dl_);
    now_ += 9 * FBP_TIME_MILLISECOND;
    fbp_dl_process(dl_);
    now_ += 1 * FBP_TIME_MILLISECOND;
    expect_send_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    fbp_dl_process(dl_);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE, status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
}

static void test_send_multiple_with_buffer_wrap(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;
    uint32_t count = (1 << 16) / sizeof(PAYLOAD_MAX);

    connect(self);
    for (uint32_t i = 0; i < count; ++i) {
        send_and_expect(self, i, i + 1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        fbp_dl_process(dl_);
        recv_link(self, FBP_FRAMER_FT_ACK_ALL, i);
    }

    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(count * sizeof(PAYLOAD_MAX), status.tx.msg_bytes);
    assert_int_equal(count * (sizeof(PAYLOAD_MAX) + FBP_FRAMER_OVERHEAD_SIZE), status.tx.bytes);
    assert_int_equal(count, status.tx.data_frames);
}

static void test_recv_and_ack(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;

    connect(self);
    expect_recv(1, PAYLOAD1, sizeof(PAYLOAD1));
    recv_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));

    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    fbp_dl_process(dl_);

    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(sizeof(PAYLOAD1), status.rx.msg_bytes);
    assert_int_equal(FBP_FRAMER_LINK_SIZE, status.tx.bytes);
    assert_int_equal(1, status.rx.data_frames);
}

static void test_recv_multiple_all_acks(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    struct fbp_dl_status_s status;
    uint32_t count = (1 << 16) / sizeof(PAYLOAD_MAX);

    connect(self);
    for (uint32_t i = 0; i < count; ++i) {
        printf("iteration %d\n", i);
        expect_recv(1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        recv_data(self, i, 1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, i);
        fbp_dl_process(dl_);
    }

    assert_int_equal(0, fbp_dl_status_get(dl_, &status));
    assert_int_equal(count * sizeof(PAYLOAD_MAX), status.rx.msg_bytes);
    assert_int_equal(count * FBP_FRAMER_LINK_SIZE, status.tx.bytes);
    assert_int_equal(count, status.rx.data_frames);
}

static void test_recv_out_of_order(void ** state) {
    struct test_s *self = (struct test_s *) *state;

    connect(self);
    expect_recv(0x11, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_data(self, 0, 0x11, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    fbp_dl_process(dl_);

    recv_data(self, 2, 0x33, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    expect_send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, 1);
    expect_send_link(self, FBP_FRAMER_FT_ACK_ONE, 2);
    fbp_dl_process(dl_);

    expect_recv(0x22, PAYLOAD1, sizeof(PAYLOAD1));
    expect_recv(0x33, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_data(self, 1, 0x22, PAYLOAD1, sizeof(PAYLOAD1));

    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 2);
    fbp_dl_process(dl_);
}

static void test_reset_retry(void ** state) {
    struct test_s *self = (struct test_s *) *state;

    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    fbp_dl_process(dl_);
    now_ += 1 * FBP_TIME_MILLISECOND;
    fbp_dl_process(dl_);
    now_ += 999 * FBP_TIME_MILLISECOND;
    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    fbp_dl_process(dl_);

    now_ += 1000 * FBP_TIME_MILLISECOND;
    expect_event(FBP_DL_EV_TX_CONNECTED);
    recv_link(self, FBP_FRAMER_FT_RESET, 1);

    fbp_dl_process(dl_);
}

int main(void) {
    hal_test_initialize();
    evm_ = fbp_evm_allocate();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initial_state, setup, teardown),
            cmocka_unit_test_setup_teardown(test_on_send_cbk, setup, teardown),
            cmocka_unit_test_setup_teardown(test_send_data_with_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(test_send_nack_resend_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(test_send_data_timeout_then_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(test_send_multiple_with_buffer_wrap, setup, teardown),
            cmocka_unit_test_setup_teardown(test_recv_and_ack, setup, teardown),
            cmocka_unit_test_setup_teardown(test_recv_multiple_all_acks, setup, teardown),
            cmocka_unit_test_setup_teardown(test_recv_out_of_order, setup, teardown),
            cmocka_unit_test_setup_teardown(test_reset_retry, setup, teardown),
            //cmocka_unit_test_setup_teardown(, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
