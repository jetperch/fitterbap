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
#include "fitterbap/comm/framer.h"
#include "fitterbap/ec.h"


#define S1 (FBP_FRAMER_SOF1)


static uint8_t GARBAGE1[] = {0x11, 0x22, 0x33, 0x44, 0x56, S1, 0x12, 0x56, 0x00, 0xFF};
static uint8_t SOF1_64[] = {S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1,
                            S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1,
                            S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1,
                            S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1, S1};
static uint8_t PAYLOAD1[] = {1, 2, 3, 4, 5, 6, 7, 8};
static uint8_t EOF[] = {FBP_FRAMER_SOF1};


struct test_s {
    struct fbp_framer_s f;
    // send truncated
    uint8_t frame1[FBP_FRAMER_MAX_SIZE];
};

static void on_data(void * user_data, uint16_t frame_id, uint16_t metadata,
                    uint8_t *msg_buffer, uint32_t msg_size) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(frame_id);
    check_expected(metadata);
    check_expected(msg_size);
    check_expected_ptr(msg_buffer);
}

static void expect_data(uint16_t frame_id, uint16_t metadata,
                        uint8_t const *msg_buffer, uint32_t msg_size) {
    expect_value(on_data, frame_id, frame_id);
    expect_value(on_data, metadata, metadata);
    expect_value(on_data, msg_size, msg_size);
    expect_memory(on_data, msg_buffer, msg_buffer, msg_size);
}

static void send_eof(struct fbp_framer_s * self) {
    fbp_framer_ll_recv(self, EOF, sizeof(EOF));
}

static void send_data(struct test_s * self, uint16_t frame_id, uint16_t metadata,
                      uint8_t const *msg_buffer, uint32_t msg_size) {
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(0, fbp_framer_construct_data(b, frame_id, metadata, msg_buffer, msg_size));
    fbp_framer_ll_recv(&self->f, b, msg_size + FBP_FRAMER_OVERHEAD_SIZE);
    expect_data(frame_id, metadata, msg_buffer, msg_size);
    send_eof(&self->f);
}

static void on_link(void * user_data, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected(frame_type);
    check_expected(frame_id);
}

static void expect_link(enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    expect_value(on_link, frame_type, frame_type);
    expect_value(on_link, frame_id, frame_id);
}

static void send_link(struct test_s * self, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    uint8_t b[FBP_FRAMER_LINK_SIZE];
    assert_int_equal(0, fbp_framer_construct_link(b, frame_type, frame_id));
    fbp_framer_ll_recv(&self->f, b, sizeof(b));
    expect_link(frame_type, frame_id);
    send_eof(&self->f);
}

static void on_framing_error(void * user_data) {
    struct test_s * self = (struct test_s *) user_data;
    (void) self;
    check_expected_ptr(self);
}

static void expect_framing_error() {
    expect_any(on_framing_error, self);
}

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    self->f.api.user_data = self;
    self->f.api.framing_error_fn = on_framing_error;
    self->f.api.link_fn = on_link;
    self->f.api.data_fn = on_data;
    fbp_framer_construct_data(self->frame1, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
    fbp_framer_reset(&self->f);
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void test_ack_all(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 0);
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
    send_link(self, FBP_FRAMER_FT_ACK_ALL, FBP_FRAMER_FRAME_ID_MAX);
}

static void test_ack_one(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_ACK_ONE, 0x12);
}

static void test_nack_frame_id(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, 0);
    send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, 1);
    send_link(self, FBP_FRAMER_FT_NACK_FRAME_ID, FBP_FRAMER_FRAME_ID_MAX);
}

static void test_nack_framing_error(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_NACK_FRAMING_ERROR, 0);
}

static void test_garbage(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_framer_ll_recv(&self->f, GARBAGE1, sizeof(GARBAGE1));
}

static void test_garbage_then_ack_all(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_framer_ll_recv(&self->f, GARBAGE1, sizeof(GARBAGE1));
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
}

static void test_sofs_garbage_sofs_link(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_framer_ll_recv(&self->f, SOF1_64, sizeof(SOF1_64));
    fbp_framer_ll_recv(&self->f, GARBAGE1, sizeof(GARBAGE1));
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
}

static void test_data(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_sofs_data(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_framer_ll_recv(&self->f, SOF1_64, sizeof(SOF1_64));
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_data_split(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    uint16_t sz = sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE;
    for (uint16_t frame_id = 1; frame_id < (sz - 1); ++frame_id) {
        assert_int_equal(0, fbp_framer_construct_data(b, frame_id, 2, PAYLOAD1, sizeof(PAYLOAD1)));
        fbp_framer_ll_recv(&self->f, b, frame_id);
        fbp_framer_ll_recv(&self->f, b + frame_id, sizeof(PAYLOAD1) + FBP_FRAMER_OVERHEAD_SIZE - frame_id);
        expect_data(frame_id, 2, PAYLOAD1, sizeof(PAYLOAD1));
        send_eof(&self->f);
    }
}

static void test_data_truncated_data(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
    expect_framing_error();
    fbp_framer_ll_recv(&self->f, self->frame1, FBP_FRAMER_HEADER_SIZE + 2);
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_link_garbage_link_sofs_data(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
    expect_framing_error();
    fbp_framer_ll_recv(&self->f, GARBAGE1, sizeof(GARBAGE1));
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 2);
    fbp_framer_ll_recv(&self->f, SOF1_64, sizeof(SOF1_64));
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_data_min_length(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t b[1] = {0x11};
    send_data(self, 1, 2, b, sizeof(b));
}

static void test_data_max_length(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t b[256];
    for (size_t i = 0; i < sizeof(b); ++i) {
        b[i] = (uint8_t) (i & 0xff);
    }
    send_data(self, 1, 2, b, sizeof(b));
}

static void test_construct_data_checks(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;
    uint8_t b[FBP_FRAMER_MAX_SIZE];
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_framer_construct_data(b, FBP_FRAMER_FRAME_ID_MAX + 1, 0, PAYLOAD1, sizeof(PAYLOAD1)));
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_framer_construct_data(b, 0, 0, PAYLOAD1, 0));
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_framer_construct_data(b, 0, 0, PAYLOAD1, FBP_FRAMER_PAYLOAD_MAX_SIZE + 1));
}

static void test_reset(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    fbp_framer_ll_recv(&self->f, self->frame1, FBP_FRAMER_HEADER_SIZE + 2);  // truncated
    fbp_framer_reset(&self->f);
    // no frame error due to reset
    send_data(self, 1, 2,  PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_truncated_flush_with_sof(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    send_link(self, FBP_FRAMER_FT_ACK_ALL, 1);
    fbp_framer_ll_recv(&self->f, self->frame1, FBP_FRAMER_HEADER_SIZE + 2);  // truncated
    expect_framing_error();
    fbp_framer_ll_recv(&self->f, SOF1_64, sizeof(SOF1_64));
    send_data(self, 1, 2, PAYLOAD1, sizeof(PAYLOAD1));
}

static void test_frame_id_subtract(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    (void) self;

    assert_int_equal(0, fbp_framer_frame_id_subtract(0, 0));
    assert_int_equal(10, fbp_framer_frame_id_subtract(12, 2));
    assert_int_equal(-10, fbp_framer_frame_id_subtract(2, 12));
    assert_int_equal(0, fbp_framer_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(10, fbp_framer_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, FBP_FRAMER_FRAME_ID_MAX - 10));
    assert_int_equal(-10, fbp_framer_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX - 10, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(1, fbp_framer_frame_id_subtract(0, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(11, fbp_framer_frame_id_subtract(10, FBP_FRAMER_FRAME_ID_MAX));
    assert_int_equal(-11, fbp_framer_frame_id_subtract(FBP_FRAMER_FRAME_ID_MAX, 10));
}

static uint8_t count_table_u8[] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

static uint8_t count_u16(uint16_t x) {
    return count_table_u8[x & 0xff] + count_table_u8[(x >> 8) & 0xff];
}

// validate that the length CRC has Hamming distance of 5 as expected.
// https://users.ece.cmu.edu/~koopman/crc/index.html
static void test_length_crc(void ** state) {
    (void) state;

    uint8_t hd = 8;
    uint8_t hd_this;

    uint16_t a16;
    uint16_t b16;
    uint8_t * a8 = (uint8_t *) &a16;
    uint8_t * b8 = (uint8_t *) &b16;

    for (int a = 0; a < 255; ++a) {
        a8[0] = (uint8_t) a;
        a8[1] = fbp_framer_length_crc(a8[0]);
        for (int b = a + 1; b < 256; ++b) {
            b8[0] = (uint8_t) b;
            b8[1] = fbp_framer_length_crc(b8[0]);
            if (a == b) {
                continue;
            }
            hd_this = count_u16(a16 ^ b16);
            if (hd_this < hd) {
                hd = hd_this;
            }
        }
    }
    assert_int_equal(5, hd);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_ack_all, setup, teardown),
            cmocka_unit_test_setup_teardown(test_ack_one, setup, teardown),
            cmocka_unit_test_setup_teardown(test_nack_frame_id, setup, teardown),
            cmocka_unit_test_setup_teardown(test_nack_framing_error, setup, teardown),
            cmocka_unit_test_setup_teardown(test_garbage, setup, teardown),
            cmocka_unit_test_setup_teardown(test_garbage_then_ack_all, setup, teardown),
            cmocka_unit_test_setup_teardown(test_sofs_garbage_sofs_link, setup, teardown),
            cmocka_unit_test_setup_teardown(test_data, setup, teardown),
            cmocka_unit_test_setup_teardown(test_sofs_data, setup, teardown),
            cmocka_unit_test_setup_teardown(test_data_split, setup, teardown),
            cmocka_unit_test_setup_teardown(test_data_truncated_data, setup, teardown),
            cmocka_unit_test_setup_teardown(test_link_garbage_link_sofs_data, setup, teardown),
            cmocka_unit_test_setup_teardown(test_data_min_length, setup, teardown),
            cmocka_unit_test_setup_teardown(test_data_max_length, setup, teardown),
            cmocka_unit_test_setup_teardown(test_construct_data_checks, setup, teardown),
            cmocka_unit_test_setup_teardown(test_reset, setup, teardown),
            cmocka_unit_test_setup_teardown(test_truncated_flush_with_sof, setup, teardown),
            cmocka_unit_test_setup_teardown(test_frame_id_subtract, setup, teardown),
            cmocka_unit_test_setup_teardown(test_length_crc, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
