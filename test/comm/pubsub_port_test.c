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
#include "fitterbap/platform.h"
#include "fitterbap/comm/pubsub_port.h"

#define PD (0x80 | FBP_PUBSUBP_MSG_PUBLISH)

static uint8_t publish_msg_null[] = {FBP_UNION_NULL, 0, 5, 'a', '/', 'v', 'g', 0, 0};
static uint8_t publish_msg_str[] = {FBP_UNION_STR, 0, 5, 'a', '/', 'v', 'g', 0, 6, 'h', 'e', 'l', 'l', 'o', 0};
static uint8_t publish_msg_json[] = {FBP_UNION_JSON, 0, 5, 'a', '/', 'v', 'g', 0, 8, '{', '"', 'a', '"', ':', '4', '}', 0};
static uint8_t publish_msg_bin[] = {FBP_UNION_BIN, 0, 5, 'a', '/', 'v', 'g', 0, 8, 1, 2, 3, 4, 5, 6, 7, 8};

static float v_f32 = 1.0f;
static double v_f64 = 1.0;
static uint8_t publish_msg_f32[] = {FBP_UNION_F32, 0, 5, 'a', '/', 'v', 'g', 0, 4, 0, 0, 0, 0};
static uint8_t publish_msg_f64[] = {FBP_UNION_F64, 0, 5, 'a', '/', 'v', 'g', 0, 8, 0, 0, 0, 0, 0, 0, 0, 0};

static uint8_t publish_msg_u8[] = {FBP_UNION_U8, 0, 5, 'a', '/', 'v', 'g', 0, 1, 42};
static uint8_t publish_msg_u16[] = {FBP_UNION_U16, 0, 5, 'a', '/', 'v', 'g', 0, 2, 42, 0};
static uint8_t publish_msg_u32[] = {FBP_UNION_U32, 0, 5, 'a', '/', 'v', 'g', 0, 4, 42, 0, 0, 0};
static uint8_t publish_msg_u64[] = {FBP_UNION_U64, 0, 5, 'a', '/', 'v', 'g', 0, 8, 42, 0, 0, 0, 0, 0, 0, 0};
static uint8_t publish_msg_i8[] = {FBP_UNION_I8, 0, 5, 'a', '/', 'v', 'g', 0, 1, 42};
static uint8_t publish_msg_i16[] = {FBP_UNION_I16, 0, 5, 'a', '/', 'v', 'g', 0, 2, 42, 0};
static uint8_t publish_msg_i32[] = {FBP_UNION_I32, 0, 5, 'a', '/', 'v', 'g', 0, 4, 42, 0, 0, 0};
static uint8_t publish_msg_i64[] = {FBP_UNION_I64, 0, 5, 'a', '/', 'v', 'g', 0, 8, 42, 0, 0, 0, 0, 0, 0, 0};

static const struct fbp_pubsubp_msg_negotiate_s NEGOTIATE_REQ = {
        .version = FBP_DL_VERSION,
        .status = 0,
        .resolution = 0,
        .msg_type = 0,
        .rsv1_u8 = 0,
        .client_connection_count = 0,
        .server_connection_count = 1,
};

static const struct fbp_pubsubp_msg_negotiate_s NEGOTIATE_RSP = {
        .version = FBP_DL_VERSION,
        .status = 0,
        .resolution = 1,
        .msg_type = 1,
        .rsv1_u8 = 0,
        .client_connection_count = 1,
        .server_connection_count = 1,
};

static uint8_t CONN_REQ[2] = {0, 0};
static uint8_t CONN_RSP[2] = {0, 1};

#include "port_hal.inc"
#define PORT_ID (2)

#define INITIALIZE(mode)                                                \
    (void) state;                                                       \
    struct test_s * self = port_hal_initialize();                       \
    expect_unsubscribe_from_all();                                      \
    self->s = fbp_pubsubp_initialize(&self->p, &self->evm_api, mode);   \
    expect_value(fbp_transport_port_register, port_id, PORT_ID);        \
    fbp_pubsubp_transport_register(self->s, PORT_ID, &self->t)

#define FINALIZE()                  \
    fbp_pubsubp_finalize(self->s);  \
    port_hal_finalize(self)


static void publish_after_connect(struct test_s * self) {
    // publish from other after connect
    expect_publish_str("a/vg", "hello");
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_PUBLISH,
                        publish_msg_str, sizeof(publish_msg_str));

    // publish to other after connect
    expect_send(PORT_ID, FBP_PUBSUBP_MSG_PUBLISH, publish_msg_str, sizeof(publish_msg_str), FBP_PUBSUBP_TIMEOUT_MS);
    fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_str("hello"));
}

static void test_server_connect_initial(void ** state) {
    INITIALIZE(FBP_PUBSUBP_MODE_DOWNSTREAM);
    expect_unsubscribe_from_all();
    expect_any_subscribe();
    expect_send(PORT_ID, FBP_PUBSUBP_MSG_NEGOTIATE, &NEGOTIATE_REQ, sizeof(NEGOTIATE_REQ), 0);
    fbp_pubsubp_on_event(self->s, FBP_DL_EV_TRANSPORT_CONNECTED);

    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_NEGOTIATE,
                        (uint8_t *) &NEGOTIATE_RSP, sizeof(NEGOTIATE_RSP));

    char * topic_list = "a" FBP_PUBSUB_UNIT_SEP_STR "b";
    expect_subscribe("", FBP_PUBSUB_SFLAG_NOPUB | FBP_PUBSUB_SFLAG_REQ);
    expect_subscribe("a", 0);
    expect_publish_str(FBP_PUBSUB_TOPIC_ADD, "a");
    expect_subscribe("b", 0);
    expect_publish_str(FBP_PUBSUB_TOPIC_ADD, "b");
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_TOPIC_LIST,
                        (uint8_t *) topic_list, sizeof(topic_list));

    expect_publish_u32("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_PUBLISH,
                        publish_msg_u32, sizeof(publish_msg_u32));

    expect_send(PORT_ID, FBP_PUBSUBP_MSG_CONNECTED, CONN_RSP, sizeof(CONN_RSP), 0);
    expect_inject(FBP_DL_EV_APP_CONNECTED);
    expect_publish_str(FBP_PUBSUB_CONN_ADD, topic_list);
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_CONNECTED,
                        CONN_REQ, sizeof(CONN_REQ));

    publish_after_connect(self);
    FINALIZE();
}

static void initialize_client(struct test_s * self) {
    expect_unsubscribe_from_all();
    struct fbp_pubsubp_msg_negotiate_s negotiate_req_client = NEGOTIATE_REQ;
    negotiate_req_client.server_connection_count = 0;
    expect_send(PORT_ID, FBP_PUBSUBP_MSG_NEGOTIATE, &negotiate_req_client, sizeof(negotiate_req_client), 0);
    fbp_pubsubp_on_event(self->s, FBP_DL_EV_TRANSPORT_CONNECTED);

    char topic_list[2] = "a";
    expect_send(PORT_ID, FBP_PUBSUBP_MSG_NEGOTIATE, (uint8_t *) &NEGOTIATE_RSP, sizeof(NEGOTIATE_RSP), 0);
    expect_query_str(FBP_PUBSUB_TOPIC_LIST, topic_list);
    expect_send(PORT_ID, FBP_PUBSUBP_MSG_TOPIC_LIST, (uint8_t *) topic_list, sizeof(topic_list), 0);
    expect_subscribe("", FBP_PUBSUB_SFLAG_RSP | FBP_PUBSUB_SFLAG_RETAIN);
    expect_publish_u32(fbp_pubsubp_feedback_topic(self->s), 1);
    expect_unsubscribe_from_all();
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_NEGOTIATE,
                        (uint8_t *) &NEGOTIATE_REQ, sizeof(NEGOTIATE_REQ));

    expect_send(PORT_ID, FBP_PUBSUBP_MSG_PUBLISH, publish_msg_u32, sizeof(publish_msg_u32), FBP_PUBSUBP_TIMEOUT_MS);
    fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_u32(42));

    expect_send(PORT_ID, FBP_PUBSUBP_MSG_CONNECTED, CONN_REQ, sizeof(CONN_REQ), 0);
    fbp_pubsubp_on_update(self->s, fbp_pubsubp_feedback_topic(self->s), &fbp_union_u32(1));

    expect_inject(FBP_DL_EV_APP_CONNECTED);
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_CONNECTED,
                        CONN_RSP, sizeof(CONN_RSP));
}

static void test_client_connect_initial(void ** state) {
    INITIALIZE(FBP_PUBSUBP_MODE_UPSTREAM);
    initialize_client(self);
    publish_after_connect(self);
    FINALIZE();
}

static void test_client_publish_when_disconnected(void ** state) {
    INITIALIZE(FBP_PUBSUBP_MODE_UPSTREAM);
    fbp_pubsubp_on_recv(self->s, PORT_ID, FBP_TRANSPORT_SEQ_SINGLE, FBP_PUBSUBP_MSG_PUBLISH,
                        publish_msg_str, sizeof(publish_msg_str));
    fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_str("hello"));
    FINALIZE();
}

static void test_serialize(void ** state) {
    INITIALIZE(FBP_PUBSUBP_MODE_UPSTREAM);
    initialize_client(self);

    memcpy(publish_msg_f32 + sizeof(publish_msg_f32) - sizeof(v_f32), &v_f32, sizeof(v_f32));
    memcpy(publish_msg_f64 + sizeof(publish_msg_f64) - sizeof(v_f64), &v_f64, sizeof(v_f64));

    expect_send(2, PD, publish_msg_null, sizeof(publish_msg_null), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_null_r()));
    expect_send(2, PD, publish_msg_str, sizeof(publish_msg_str), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_cstr_r((char*) (publish_msg_str + 9))));
    expect_send(2, PD, publish_msg_json, sizeof(publish_msg_json), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_cjson_r((char*) (publish_msg_json + 9))));
    expect_send(2, PD, publish_msg_bin, sizeof(publish_msg_bin), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_cbin_r(publish_msg_bin + 9, 8)));

    expect_send(2, PD, publish_msg_f32, sizeof(publish_msg_f32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_f32_r(v_f32)));
    expect_send(2, PD, publish_msg_f64, sizeof(publish_msg_f64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_f64_r(v_f64)));

    expect_send(2, PD, publish_msg_u8, sizeof(publish_msg_u8), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_u8_r(42)));
    expect_send(2, PD, publish_msg_u16, sizeof(publish_msg_u16), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_u16_r(42)));
    expect_send(2, PD, publish_msg_u32, sizeof(publish_msg_u32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_u32_r(42)));
    expect_send(2, PD, publish_msg_u64, sizeof(publish_msg_u64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_u64_r(42)));

    expect_send(2, PD, publish_msg_i8, sizeof(publish_msg_i8), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_i8_r(42)));
    expect_send(2, PD, publish_msg_i16, sizeof(publish_msg_i16), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_i16_r(42)));
    expect_send(2, PD, publish_msg_i32, sizeof(publish_msg_i32), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_i32_r(42)));
    expect_send(2, PD, publish_msg_i64, sizeof(publish_msg_i64), FBP_PUBSUBP_TIMEOUT_MS);
    assert_int_equal(0, fbp_pubsubp_on_update(self->s, "a/vg", &fbp_union_i64_r(42)));

    FINALIZE();
}

static void test_deserialize(void ** state) {
    INITIALIZE(FBP_PUBSUBP_MODE_UPSTREAM);
    initialize_client(self);

    expect_publish_null("a/vg");
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_null, sizeof(publish_msg_null));
    expect_publish_str("a/vg", (char *) publish_msg_str + 9);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_str, sizeof(publish_msg_str));
    expect_publish_json("a/vg", (char *) publish_msg_json + 9);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_json, sizeof(publish_msg_json));
    expect_publish_bin("a/vg", (char *) publish_msg_bin + 9, 8);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_bin, sizeof(publish_msg_bin));

    expect_publish_f32("a/vg", v_f32);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_f32, sizeof(publish_msg_f32));
    expect_publish_f64("a/vg", v_f64);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_f64, sizeof(publish_msg_f64));

    expect_publish_u8("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_u8, sizeof(publish_msg_u8));
    expect_publish_u16("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_u16, sizeof(publish_msg_u16));
    expect_publish_u32("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_u32, sizeof(publish_msg_u32));
    expect_publish_u64("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_u64, sizeof(publish_msg_u64));

    expect_publish_i8("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_i8, sizeof(publish_msg_i8));
    expect_publish_i16("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_i16, sizeof(publish_msg_i16));
    expect_publish_i32("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_i32, sizeof(publish_msg_i32));
    expect_publish_i64("a/vg", 42);
    fbp_pubsubp_on_recv(self->s, 2, FBP_TRANSPORT_SEQ_SINGLE, PD, publish_msg_i64, sizeof(publish_msg_i64));

    FINALIZE();
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_server_connect_initial),
            cmocka_unit_test(test_client_connect_initial),
            cmocka_unit_test(test_client_publish_when_disconnected),
            cmocka_unit_test(test_serialize),
            cmocka_unit_test(test_deserialize),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
