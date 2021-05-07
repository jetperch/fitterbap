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
#include "fitterbap/platform.h"
#include "fitterbap/comm/pubsub_port.h"

static uint8_t publish_msg_null[] = {5, 'a', '/', 'v', 'g', 0, 0};
static uint8_t publish_msg_str[] = {5, 'a', '/', 'v', 'g', 0, 6, 'h', 'e', 'l', 'l', 'o', 0};
static uint8_t publish_msg_json[] = {5, 'a', '/', 'v', 'g', 0, 8, '{', '"', 'a', '"', ':', '4', '}', 0};
static uint8_t publish_msg_bin[] = {5, 'a', '/', 'v', 'g', 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};

static float v_f32 = 1.0f;
static double v_f64 = 1.0;
static uint8_t publish_msg_f32[] = {5, 'a', '/', 'v', 'g', 0, 4, 0, 0, 0, 0};
static uint8_t publish_msg_f64[] = {5, 'a', '/', 'v', 'g', 0, 8, 0, 0, 0, 0, 0, 0, 0, 0};

static uint8_t publish_msg_u8[] = {5, 'a', '/', 'v', 'g', 0, 1, 42};
static uint8_t publish_msg_u16[] = {5, 'a', '/', 'v', 'g', 0, 2, 42, 0};
static uint8_t publish_msg_u32[] = {5, 'a', '/', 'v', 'g', 0, 4, 42, 0, 0, 0};
static uint8_t publish_msg_u64[] = {5, 'a', '/', 'v', 'g', 0, 8, 42, 0, 0, 0, 0, 0, 0, 0};

#include "port_hal.inc"

static void test_server_connect(void ** state) {
    (void) state;
    struct test_s self;
    self.s = fbp_pubsubp_initialize(&self.p, FBP_PUBSUBP_MODE_DOWNSTREAM);  // server
    expect_value(fbp_transport_port_register, port_id, 2);
    fbp_pubsubp_transport_register(self.s, 2, &self.t);

    uint64_t server_msg_conn[3] = {0, 1, 0};
    expect_send(2, FBP_PUBSUBP_MSG_CONN, server_msg_conn, sizeof(server_msg_conn), FBP_PUBSUBP_TIMEOUT_MS);
    fbp_pubsubp_on_event(self.s, FBP_DL_EV_TX_CONNECTED);
    fbp_pubsubp_finalize(self.s);
}

#define SEP FBP_PUBSUB_UNIT_SEP_STR

static void init(struct test_s * self) {
    self->s = fbp_pubsubp_initialize(&self->p, FBP_PUBSUBP_MODE_UPSTREAM);  // client
    expect_value(fbp_transport_port_register, port_id, 2);
    fbp_pubsubp_transport_register(self->s, 2, &self->t);
}

static void tx_connect(struct test_s * self) {
    fbp_pubsubp_on_event(self->s, FBP_DL_EV_TX_CONNECTED);
}

static void msg_conn(struct test_s * self, uint64_t server_count) {
    uint64_t server_msg_conn[3] = {0, server_count, 0};
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_CONN, (uint8_t *) server_msg_conn, sizeof(server_msg_conn));
}

static void expect_conn(struct test_s * self, uint64_t server_count, uint64_t client_count) {
    (void) self;
    uint64_t client_msg_conn[3] = {1, server_count, client_count};
    expect_send(2, FBP_PUBSUBP_MSG_CONN, client_msg_conn, sizeof(client_msg_conn), FBP_PUBSUBP_TIMEOUT_MS);
    expect_subscribe("");
}

static void send_topic_list(struct test_s * self) {
    char topic_list[] = "a" SEP "b";
    expect_send(2, FBP_PUBSUBP_MSG_TOPIC_LIST, topic_list, sizeof(topic_list), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, FBP_PUBSUB_TOPIC_LIST, &fbp_union_cstr_r(topic_list)));
}

static void initialize_client(struct test_s * self) {
    init(self);
    tx_connect(self);
    expect_conn(self, 1, 1);
    msg_conn(self, 1);
    send_topic_list(self);
}

static void test_client_msg_after_connect(void ** state) {
    (void) state;
    struct test_s self;
    uint16_t publish_port_data = FBP_PUBSUBP_MSG_PUBLISH | (FBP_UNION_U32 << 8) | (FBP_UNION_FLAG_RETAIN << 13);
    init(&self);
    tx_connect(&self);

    // not yet connected, do not send or receive topics
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/v", &fbp_union_cstr_r("hello")));
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, publish_port_data, publish_msg_u32, sizeof(publish_msg_u32));

    expect_conn(&self, 1, 1);
    msg_conn(&self, 1);
    send_topic_list(&self);
    expect_send(2, FBP_PUBSUBP_MSG_TOPIC_ADD, "c", 2, FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, FBP_PUBSUB_TOPIC_ADD, &fbp_union_cstr_r("c")));
    expect_send(2, FBP_PUBSUBP_MSG_TOPIC_REMOVE, "b", 2, FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, FBP_PUBSUB_TOPIC_REMOVE, &fbp_union_cstr_r("b")));

    // now connected, send
    expect_send(2, publish_port_data, publish_msg_u32, sizeof(publish_msg_u32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_u32_r(42)));

    // now connected, receive
    expect_publish_u32("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, publish_port_data, publish_msg_u32, sizeof(publish_msg_u32));

    fbp_pubsubp_finalize(self.s);
}

static void test_client_msg_before_connect(void ** state) {
    (void) state;
    struct test_s self;
    init(&self);
    msg_conn(&self, 1);
    expect_conn(&self, 1, 1);
    tx_connect(&self);
    fbp_pubsubp_finalize(self.s);
}

#define PD(dtype) FBP_PUBSUBP_MSG_PUBLISH | (dtype << 8) | (FBP_UNION_FLAG_RETAIN << 13)

static void test_serialize(void ** state) {
    (void) state;
    struct test_s self;
    initialize_client(&self);

    memcpy(publish_msg_f32 + sizeof(publish_msg_f32) - sizeof(v_f32), &v_f32, sizeof(v_f32));
    memcpy(publish_msg_f64 + sizeof(publish_msg_f64) - sizeof(v_f64), &v_f64, sizeof(v_f64));

    expect_send(2, PD(FBP_UNION_NULL), publish_msg_null, sizeof(publish_msg_null), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_null_r()));
    expect_send(2, PD(FBP_UNION_STR), publish_msg_str, sizeof(publish_msg_str), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_cstr_r((char*) (publish_msg_str + 7))));
    expect_send(2, PD(FBP_UNION_JSON), publish_msg_json, sizeof(publish_msg_json), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_cjson_r((char*) (publish_msg_json + 7))));
    expect_send(2, PD(FBP_UNION_BIN), publish_msg_bin, sizeof(publish_msg_bin), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_cbin_r(publish_msg_bin + 7, 8)));

    expect_send(2, PD(FBP_UNION_F32), publish_msg_f32, sizeof(publish_msg_f32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_f32_r(v_f32)));
    expect_send(2, PD(FBP_UNION_F64), publish_msg_f64, sizeof(publish_msg_f64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_f64_r(v_f64)));

    expect_send(2, PD(FBP_UNION_U8), publish_msg_u8, sizeof(publish_msg_u8), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_u8_r(42)));
    expect_send(2, PD(FBP_UNION_U16), publish_msg_u16, sizeof(publish_msg_u16), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_u16_r(42)));
    expect_send(2, PD(FBP_UNION_U32), publish_msg_u32, sizeof(publish_msg_u32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_u32_r(42)));
    expect_send(2, PD(FBP_UNION_U64), publish_msg_u64, sizeof(publish_msg_u64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_u64_r(42)));

    expect_send(2, PD(FBP_UNION_I8), publish_msg_u8, sizeof(publish_msg_u8), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_i8_r(42)));
    expect_send(2, PD(FBP_UNION_I16), publish_msg_u16, sizeof(publish_msg_u16), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_i16_r(42)));
    expect_send(2, PD(FBP_UNION_I32), publish_msg_u32, sizeof(publish_msg_u32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_i32_r(42)));
    expect_send(2, PD(FBP_UNION_I64), publish_msg_u64, sizeof(publish_msg_u64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self.s, "a/vg", &fbp_union_i64_r(42)));

    fbp_pubsubp_finalize(self.s);
}

static void test_deserialize(void ** state) {
    (void) state;
    struct test_s self;
    initialize_client(&self);

    expect_publish_null("a/vg");
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_NULL), publish_msg_null, sizeof(publish_msg_null));
    expect_publish_str("a/vg", (char *) publish_msg_str + 7);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_STR), publish_msg_str, sizeof(publish_msg_str));
    expect_publish_json("a/vg", (char *) publish_msg_json + 7);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_JSON), publish_msg_json, sizeof(publish_msg_json));
    expect_publish_bin("a/vg", (char *) publish_msg_bin + 7, 8);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_BIN), publish_msg_bin, sizeof(publish_msg_bin));

    expect_publish_f32("a/vg", v_f32);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_F32), publish_msg_f32, sizeof(publish_msg_f32));
    expect_publish_f64("a/vg", v_f64);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_F64), publish_msg_f64, sizeof(publish_msg_f64));

    expect_publish_u8("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_U8), publish_msg_u8, sizeof(publish_msg_u8));
    expect_publish_u16("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_U16), publish_msg_u16, sizeof(publish_msg_u16));
    expect_publish_u32("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_U32), publish_msg_u32, sizeof(publish_msg_u32));
    expect_publish_u64("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_U64), publish_msg_u64, sizeof(publish_msg_u64));

    expect_publish_i8("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_I8), publish_msg_u8, sizeof(publish_msg_u8));
    expect_publish_i16("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_I16), publish_msg_u16, sizeof(publish_msg_u16));
    expect_publish_i32("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_I32), publish_msg_u32, sizeof(publish_msg_u32));
    expect_publish_i64("a/vg", 42);
    fbp_pubsubp_on_recv(self.s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD(FBP_UNION_I64), publish_msg_u64, sizeof(publish_msg_u64));

    fbp_pubsubp_finalize(self.s);
}

#if 0
static void test_pubsub_to_transport(void ** state) {
    struct test_s * self = (struct test_s *) *state;

    struct fbp_union_s value;
    value.type = FBP_UNION_U32;
    value.value.u32 = 42;
    uint8_t msg[] = {3, 'h', '/', 'w', 0, 4, 42, 0, 0, 0};
    uint16_t port_data = FBP_PUBSUBP_MSG_PUBLISH | (((uint16_t) value.type) << 8);
    expect_send(2, port_data, msg, sizeof(msg));
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "h/w", &value));
}

static void test_transport_to_pubsub(void ** state) {
    struct test_s * self = (struct test_s *) *state;
    struct fbp_union_s value;
    value.type = FBP_UNION_U32;
    value.value.u32 = 42;
    uint8_t msg[] = {3 , 'h', '/', 'w', 0, 4, 42, 0, 0, 0};
    expect_publish_u32("h/w", 42);
    uint16_t port_data = FBP_PUBSUBP_MSG_PUBLISH | (((uint16_t) value.type) << 8);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, port_data, msg, sizeof(msg));
}
#endif

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_server_connect),
            cmocka_unit_test(test_client_msg_after_connect),
            cmocka_unit_test(test_client_msg_before_connect),
            cmocka_unit_test(test_serialize),
            cmocka_unit_test(test_deserialize),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
