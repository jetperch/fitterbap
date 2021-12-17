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
#include "fitterbap/comm/data_link.h"
#include "fitterbap/collections/ring_buffer_u64.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/time.h"
#include <stdio.h>

#define SEND_BUFFER_SIZE (1 << 13)
static uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
static uint8_t PAYLOAD2[] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
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

struct test_s {
    struct fbp_evm_s * evm;
    struct fbp_dl_s * dl;
    struct fbp_framer_s * f;
    uint32_t send_available;
    int64_t now;
};

static int64_t ll_time_get(struct fbp_evm_s * evm) {
    struct test_s * self = (struct test_s *) evm;
    return self->now;
}

static int32_t ll_schedule_fn(
        struct fbp_evm_s * evm, int64_t timestamp,
        fbp_evm_callback cbk_fn, void * cbk_user_data) {
    struct test_s * self = (struct test_s *) evm;
    return fbp_evm_schedule(self->evm, timestamp, cbk_fn, cbk_user_data);
}

static int32_t ll_cancel_fn(struct fbp_evm_s * evm, int32_t event_id) {
    struct test_s * self = (struct test_s *) evm;
    return fbp_evm_cancel(self->evm, event_id);
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    if ((buffer_size == 1) && (buffer[0] == FBP_FRAMER_SOF1)) {
        return;  // EOF
    } else if (buffer[2] & 0xF8) {
        while (buffer_size) {
            uint8_t event = (buffer[2] >> 3) & 0x1f;
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
    struct test_s * self = (struct test_s *) user_data;
    return self->send_available;  // improve?
}

static void on_event(void *user_data, enum fbp_dl_event_e event) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(event);
}

#define expect_event(expected_event)  expect_value(on_event, event, (expected_event))

static void on_recv(void *user_data, uint16_t metadata, uint8_t *msg_buffer, uint32_t msg_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(metadata);
    check_expected(msg_size);
    check_expected_ptr(msg_buffer);
}

static void expect_recv(uint16_t metadata, uint8_t *msg_buffer, uint32_t msg_size) {
    expect_value(on_recv, metadata, metadata);
    expect_value(on_recv, msg_size, msg_size);
    expect_memory(on_recv, msg_buffer, msg_buffer, msg_size);
}

static struct test_s * setup() {
    struct test_s * self = test_calloc(1, sizeof(struct test_s));
    self->evm = fbp_evm_allocate();
    self->f = fbp_framer_initialize();
    struct fbp_evm_api_s evm_api = {
        .evm = (struct fbp_evm_s *) self,  // use wrappers
        .timestamp = ll_time_get,
        .schedule = ll_schedule_fn,
        .cancel = ll_cancel_fn
    };

    struct fbp_dl_config_s config = {
        .tx_window_size = 16,
        .rx_window_size = 16,
        .tx_timeout = 10 * FBP_TIME_MILLISECOND,
        .tx_link_size = 16,
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

    self->now = 0;
    self->send_available = FBP_FRAMER_MAX_SIZE;
    self->dl = fbp_dl_initialize(&config, &evm_api, &ll, self->f);
    assert_non_null(self->dl);
    fbp_dl_register_upper_layer(self->dl, &ul);
    return self;
}

static void teardown(struct test_s * self) {
    fbp_dl_finalize(self->dl);
    self->f->finalize(self->f);
    fbp_evm_free(self->evm);
    memset(self, 0, sizeof(*self));
    test_free(self);
}

#define SETUP()                             \
    (void) state;                           \
    struct test_s * self = setup();         \
    struct fbp_dl_status_s status;          \
    (void) status  /* for test cases where it's not used */

#define TEARDOWN() teardown(self)

static void test_initial_state(void ** state) {
    SETUP();
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(FBP_DL_VERSION, status.version);
    assert_int_equal(0, status.rx.msg_bytes);
    assert_int_equal(0, status.rx.data_frames);
    assert_int_equal(0, status.rx_framer.total_bytes);
    assert_int_equal(0, status.rx_framer.ignored_bytes);
    assert_int_equal(0, status.rx_framer.resync);
    assert_int_equal(0, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);
    TEARDOWN();
}

static void test_frame_id_subtract(void ** state) {
    (void) state;
    assert_int_equal(0, fbp_dl_frame_id_subtract(0, 0));
    assert_int_equal(10, fbp_dl_frame_id_subtract(12, 2));
    assert_int_equal(-10, fbp_dl_frame_id_subtract(2, 12));
    assert_int_equal(0, fbp_dl_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(10, fbp_dl_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, FBP_FRAMER_FRAME_ID_MAX - 10));
    assert_int_equal(-10, fbp_dl_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX - 10, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(1, fbp_dl_frame_id_subtract(0, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(11, fbp_dl_frame_id_subtract(10, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(-11, fbp_dl_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, 10));
}

static void expect_send_data(struct test_s *self,
                             uint16_t frame_id, uint16_t metadata,
                             uint8_t *msg_buffer, uint32_t msg_size) {
    (void) self;
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(0, self->f->construct_data(self->f, b, frame_id, metadata, msg_buffer, msg_size));
    uint16_t frame_sz = msg_size + FBP_FRAMER_OVERHEAD_SIZE;
    expect_value(ll_send, buffer_size, frame_sz);
    expect_memory(ll_send, buffer, b, frame_sz);
}

static void send_and_expect(struct test_s *self,
                 uint16_t frame_id, uint16_t metadata,
                 uint8_t *msg_buffer, uint32_t msg_size) {
    expect_send_data(self, frame_id, metadata, msg_buffer, msg_size);
    assert_int_equal(0, fbp_dl_send(self->dl, metadata, msg_buffer, msg_size, 0));
}

static void expect_send_link(struct test_s *self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    (void) self;
    uint64_t u64 = 0;
    assert_int_equal(0, self->f->construct_link(self->f, &u64, frame_type, frame_id));
    expect_value(ll_send, event, frame_type);
    expect_value(ll_send, frame_id, frame_id);
}

static void recv_eof(struct test_s *self) {
    uint8_t eof[] = {FBP_FRAMER_SOF1};
    fbp_dl_ll_recv(self->dl, eof, sizeof(eof));
}

static void recv_link(struct test_s *self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    uint64_t b;
    assert_int_equal(0, self->f->construct_link(self->f, &b, frame_type, frame_id));
    fbp_dl_ll_recv(self->dl, (uint8_t *) &b, sizeof(b));
}

static void recv_data(struct test_s *self, uint16_t frame_id, uint16_t metadata,
                      uint8_t *msg_buffer, uint32_t msg_size) {
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(0, self->f->construct_data(self->f, b, frame_id, metadata, msg_buffer, msg_size));
    uint16_t frame_sz = msg_size + FBP_FRAMER_OVERHEAD_SIZE;
    fbp_dl_ll_recv(self->dl, b, frame_sz);
}

static void process_now(struct test_s *self) {
    fbp_evm_process(self->evm, self->now);
}

static void process_n(struct test_s *self, int32_t n) {
    while ((n > 0) && (FBP_TIME_MAX != fbp_evm_time_next(self->evm))) {
        self->now = fbp_evm_time_next(self->evm);
        fbp_evm_process(self->evm, self->now);
        --n;
    }
}

static void connect(struct test_s *self) {
    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    self->now += 10 * FBP_TIME_MILLISECOND;
    process_now(self);
    recv_link(self, FBP_FRAMER_FT_RESET, 1);
    expect_event(FBP_DL_EV_CONNECTED);
    recv_eof(self);
    self->now += 10 * FBP_TIME_MILLISECOND;
    process_now(self);
    fbp_dl_status_clear(self->dl);
}

void on_schedule_fn(void * user_data, int64_t next_time) {
    (void) user_data;
    check_expected(next_time);
}

static void test_send_when_not_connected(void ** state) {
    SETUP();
    assert_int_equal(FBP_ERROR_UNAVAILABLE, fbp_dl_send(self->dl, 1, PAYLOAD1, sizeof(PAYLOAD1), 0));
    TEARDOWN();
}

static void test_on_invalid_send(void ** state) {
    SETUP();
    connect(self);
    //assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_dl_send(self->dl, 0, PAYLOAD1, FBP_FRAMER_PAYLOAD_MAX_SIZE + 1, 0));
    TEARDOWN();
}

static void test_on_send_cbk(void ** state) {
    SETUP();
    connect(self);
    fbp_evm_register_schedule_callback(self->evm, on_schedule_fn, self);
    expect_value(on_schedule_fn, next_time, 0x147ae18);
    assert_int_equal(0, fbp_dl_send(self->dl, 1, PAYLOAD1, sizeof(PAYLOAD1), 0));
    TEARDOWN();
}

static void test_send_data_with_ack(void ** state) {
    SETUP();
    connect(self);
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    self->now += FBP_TIME_MILLISECOND * 4;
    process_now(self);

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE + 1, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(0, status.tx.data_frames);

    recv_eof(self);
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(1, status.tx.data_frames);
    TEARDOWN();
}

static void test_send_2data_with_2ack(void ** state) {
    SETUP();
    connect(self);
    fbp_dl_tx_window_set(self->dl, 128);
    send_and_expect(self, 0, 10, PAYLOAD1, sizeof(PAYLOAD1));
    send_and_expect(self, 1, 11, PAYLOAD2, sizeof(PAYLOAD2));
    process_n(self, 2);

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(sizeof(PAYLOAD1) + sizeof(PAYLOAD2) + 2 * FBP_FRAMER_OVERHEAD_SIZE + 1, status.tx.bytes);
    assert_int_equal(0, status.tx.data_frames);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
    recv_eof(self);

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(2, status.tx.data_frames);
    TEARDOWN();
}

static void test_send_two_before_tx_window_set(void ** state) {
    SETUP();
    connect(self);
    send_and_expect(self, 0, 10, PAYLOAD1, sizeof(PAYLOAD1));
    process_n(self, 1);
    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    recv_eof(self);

    send_and_expect(self, 1, 11, PAYLOAD1, sizeof(PAYLOAD1));
    assert_int_equal(FBP_ERROR_FULL, fbp_dl_send(self->dl, 12, PAYLOAD2, sizeof(PAYLOAD2), 0));
    fbp_dl_tx_window_set(self->dl, 16);
    send_and_expect(self, 2, 13, PAYLOAD2, sizeof(PAYLOAD2));
    process_n(self, 2);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 2);
    recv_eof(self);

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(3, status.tx.data_frames);
    TEARDOWN();
}

static void test_send_nack_resend_ack(void ** state) {
    SETUP();
    connect(self);
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    process_n(self, 1);

    recv_link(self, FBP_FRAMER_FT_NACK_FRAMING_ERROR, 0);
    recv_eof(self);
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(0, status.tx.data_frames);

    expect_send_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));  // due to nack
    process_n(self, 1);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    recv_eof(self);
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(1, status.tx.data_frames);
    TEARDOWN();
}

static void test_send_data_timeout_then_ack(void ** state) {
    SETUP();
    connect(self);
    self->now = 5 * FBP_TIME_MILLISECOND;
    send_and_expect(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    process_now(self);
    self->now += 9 * FBP_TIME_MILLISECOND;
    process_now(self);
    self->now += 1 * FBP_TIME_MILLISECOND;
    expect_send_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    process_now(self);

    recv_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    recv_eof(self);
    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(2 * (sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE + 1), status.tx.bytes);
    assert_int_equal(1, status.tx.data_frames);
    TEARDOWN();
}

static void test_send_multiple_with_buffer_wrap(void ** state) {
    SETUP();
    uint32_t count = (1 << 16) / sizeof(PAYLOAD_MAX);
    connect(self);
    for (uint32_t i = 0; i < count; ++i) {
        send_and_expect(self, i, i + 1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        process_n(self, 1);
        recv_link(self, FBP_FRAMER_FT_ACK_ALL, i);
        recv_eof(self);
    }

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(count * sizeof(PAYLOAD_MAX), status.tx.msg_bytes);
    assert_int_equal(count * (sizeof(PAYLOAD_MAX) + FBP_FRAMER_OVERHEAD_SIZE + 1), status.tx.bytes);
    assert_int_equal(count, status.tx.data_frames);
    TEARDOWN();
}

static void test_recv_and_ack(void ** state) {
    SETUP();
    connect(self);
    recv_data(self, 0, 1, PAYLOAD1, sizeof(PAYLOAD1));
    expect_recv(1, PAYLOAD1, sizeof(PAYLOAD1));
    recv_eof(self);

    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    process_n(self, 5);

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(sizeof(PAYLOAD1), status.rx.msg_bytes);
    assert_int_equal(FBP_FRAMER_LINK_SIZE + 1, status.tx.bytes);
    assert_int_equal(1, status.rx.data_frames);
    TEARDOWN();
}

static void test_recv_multiple_all_acks(void ** state) {
    SETUP();
    uint32_t count = (1 << 16) / sizeof(PAYLOAD_MAX);
    connect(self);
    for (uint32_t i = 0; i < count; ++i) {
        // printf("iteration %d\n", i);
        expect_recv(1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        recv_data(self, i, 1, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
        recv_eof(self);
        expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, i);
        process_n(self, 5);
    }

    assert_int_equal(0, fbp_dl_status_get(self->dl, &status));
    assert_int_equal(count * sizeof(PAYLOAD_MAX), status.rx.msg_bytes);
    assert_int_equal(count * (FBP_FRAMER_LINK_SIZE + 1), status.tx.bytes);
    assert_int_equal(count, status.rx.data_frames);
    TEARDOWN();
}

static void test_recv_out_of_order(void ** state) {
    SETUP();

    connect(self);
    expect_recv(0x11, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_data(self, 0, 0x11, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_eof(self);
    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    process_n(self, 5);

    recv_data(self, 2, 0x33, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_eof(self);
    expect_send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, 1);
    expect_send_link(self, FBP_FRAMER_FT_ACK_ONE, 2);
    process_n(self, 5);

    expect_recv(0x22, PAYLOAD1, sizeof(PAYLOAD1));
    expect_recv(0x33, PAYLOAD_MAX, sizeof(PAYLOAD_MAX));
    recv_data(self, 1, 0x22, PAYLOAD1, sizeof(PAYLOAD1));
    recv_eof(self);

    expect_send_link(self, FBP_FRAMER_FT_ACK_ALL, 2);
    process_n(self, 5);
    TEARDOWN();
}

static void test_reset_retry(void ** state) {
    SETUP();
    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    process_n(self, 1);
    self->now += 1 * FBP_TIME_MILLISECOND;
    process_now(self);
    self->now += 999 * FBP_TIME_MILLISECOND;
    expect_send_link(self, FBP_FRAMER_FT_RESET, 0);
    process_now(self);

    self->now += 1000 * FBP_TIME_MILLISECOND;
    expect_event(FBP_DL_EV_CONNECTED);
    recv_link(self, FBP_FRAMER_FT_RESET, 1);
    recv_eof(self);
    process_now(self);
    TEARDOWN();
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_initial_state),
            cmocka_unit_test(test_frame_id_subtract),
            cmocka_unit_test(test_send_when_not_connected),
            cmocka_unit_test(test_on_invalid_send),
            cmocka_unit_test(test_on_send_cbk),
            cmocka_unit_test(test_send_data_with_ack),
            cmocka_unit_test(test_send_2data_with_2ack),
            cmocka_unit_test(test_send_two_before_tx_window_set),
            cmocka_unit_test(test_send_nack_resend_ack),
            cmocka_unit_test(test_send_data_timeout_then_ack),
            cmocka_unit_test(test_send_multiple_with_buffer_wrap),
            cmocka_unit_test(test_recv_and_ack),
            cmocka_unit_test(test_recv_multiple_all_acks),
            cmocka_unit_test(test_recv_out_of_order),
            cmocka_unit_test(test_reset_retry),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
