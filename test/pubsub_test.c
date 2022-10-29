/*
 * Copyright 2014-2022 Jetperch LLC
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
#include "fitterbap/ec.h"
#include "fitterbap/platform.h"
#include "fitterbap/pubsub.h"

#include <stdio.h>
#include <stdlib.h>

static const char * META1 =
    "{"
        "\"dtype\": \"u32\","
        "\"brief\": \"value1\","
        "\"default\": \"42\","
        "\"options\": [[42, \"v1\"], [43, \"v2\"]],"
        "\"flags\": []"
    "}";

static const char * META2 = "{"
    "\"dtype\": \"u8\","
    "\"brief\": \"Number selection.\","
    "\"default\": 2,"
    "\"options\": ["
        "[0, \"zero\"],"
        "[1, \"one\"],"
        "[2, \"two\"],"
        "[3, \"three\", \"3\"],"
        "[4, \"four\"],"
        "[5, \"five\"],"
        "[6, \"six\"],"
        "[7, \"seven\"],"
        "[8, \"eight\"],"
        "[9, \"nine\"],"
        "[10, \"ten\"]"
    "]"
"}";

static int setup(void ** state) {
    (void) state;
    return 0;
}

static int teardown(void ** state) {
    (void) state;
    fflush(stdout);
    fflush(stderr);
    return 0;
}

static uint8_t on_pub(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) user_data;
    check_expected_ptr(topic);
    int type = value->type;
    // int flags = value->flags;
    check_expected(type);
    const char * cstr = value->value.str;
    float f32 = value->value.f32;
    double f64 = value->value.f64;
    uint8_t u8 = value->value.u8;
    uint16_t u16 = value->value.u16;
    uint32_t u32 = value->value.u32;
    uint64_t u64 = value->value.u64;
    int8_t i8 = value->value.i8;
    int16_t i16 = value->value.i16;
    int32_t i32 = value->value.i32;
    int64_t i64 = value->value.i64;
    switch (type) {
        case FBP_UNION_NULL:
            break;
        case FBP_UNION_STR:     // intentional fall-through
        case FBP_UNION_JSON:    // intentional fall-through
        case FBP_UNION_BIN:
            check_expected_ptr(cstr);
            break;
        case FBP_UNION_F32: check_expected(f32); break;
        case FBP_UNION_F64: check_expected(f64); break;
        case FBP_UNION_U8: check_expected(u8); break;
        case FBP_UNION_U16: check_expected(u16); break;
        case FBP_UNION_U32: check_expected(u32); break;
        case FBP_UNION_U64: check_expected(u64); break;
        case FBP_UNION_I8: check_expected(i8); break;
        case FBP_UNION_I16: check_expected(i16); break;
        case FBP_UNION_I32: check_expected(i32); break;
        case FBP_UNION_I64: check_expected(i64); break;
        default:
            assert_false(1);
            break;
    }
    return 0;
}

#define expect_pub_null(topic_str)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_NULL);

#define expect_pub_cstr(topic_str, value)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_STR); \
    expect_string(on_pub, cstr, value)

#define expect_pub_json(topic_str, value)  \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_JSON); \
    expect_string(on_pub, cstr, value)

#define expect_pub_f32(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_F32); \
    expect_value(on_pub, f32, value)

#define expect_pub_f64(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_F64); \
    expect_value(on_pub, f64, value)

#define expect_pub_u8(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_U8); \
    expect_value(on_pub, u8, value)

#define expect_pub_u16(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_U16); \
    expect_value(on_pub, u16, value)

#define expect_pub_u32(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_U32); \
    expect_value(on_pub, u32, value)

#define expect_pub_u64(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_U64); \
    expect_value(on_pub, u64, value)

#define expect_pub_i8(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_I8); \
    expect_value(on_pub, i8, value)

#define expect_pub_i16(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_I16); \
    expect_value(on_pub, i16, value)

#define expect_pub_i32(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_I32); \
    expect_value(on_pub, i32, value)

#define expect_pub_i64(topic_str, value) \
    expect_string(on_pub, topic, topic_str); \
    expect_value(on_pub, type, FBP_UNION_I64); \
    expect_value(on_pub, i64, value)

static uint8_t on_pub_never(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) user_data;
    (void) topic;
    (void) value;
    assert_true(0);
    return 0;
}

static void test_initialize(void ** state) {
    (void) state;

    struct fbp_pubsub_s *ps = fbp_pubsub_initialize("s", 0);
    assert_string_equal("s", fbp_pubsub_topic_prefix(ps));
    fbp_pubsub_finalize(ps);
}

static void test_cstr(void ** state) {
    (void) state;

    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_non_null(ps);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("hello world"), NULL, NULL));

    // subscribe directly, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));

    // subscribe to parent, get retained value
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));

    // publish
    expect_pub_cstr("s/hello/world", "there"); // first subscription
    expect_pub_cstr("s/hello/world", "there"); // second subscription
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("there"), NULL, NULL));

    // subscribe directly, without retained flag value, expect no retained
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB, on_pub_never, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_str(void ** state) {
    (void) state;
    char msg[16] = "hello world";

    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 128);
    assert_non_null(ps);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    expect_pub_cstr("s/hello/world", "hello world");
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_str(msg), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_str_but_too_big(void ** state) {
    (void) state;
    char msg[] = "hello world, this is a very long message that will exceed the buffer size";

    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 32);
    assert_non_null(ps);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_str(msg), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_str_full_buffer(void ** state) {
    (void) state;
    char msg[] = "0123456789abcde";
    fbp_os_mutex_t mutex = fbp_os_mutex_alloc("pubsub");
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 32);
    fbp_pubsub_register_mutex(ps, mutex);

    assert_non_null(ps);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_str(msg), NULL, NULL));
    assert_int_equal(FBP_ERROR_NOT_ENOUGH_MEMORY, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_str(msg), NULL, NULL));
    expect_pub_cstr("s/hello/world", msg);
    fbp_pubsub_process(ps);

    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_str(msg), NULL, NULL));
    expect_pub_cstr("s/hello/world", msg);
    fbp_pubsub_process(ps);

    fbp_pubsub_finalize(ps);
    fbp_os_mutex_free(mutex);
}

static void test_integers(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/v", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));

    expect_pub_u8("s/v", 0xff);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_u8_r(0xff), NULL, NULL));

    expect_pub_u16("s/v", 0xffff);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_u16_r(0xffff), NULL, NULL));

    expect_pub_u32("s/v", 0xffffffff);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_u32_r(0xffffffff), NULL, NULL));

    expect_pub_u64("s/v", 0xffffffffffffffffLL);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_u64_r(0xffffffffffffffffLL), NULL, NULL));

    expect_pub_i8("s/v", -127);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_i8_r(-127), NULL, NULL));

    expect_pub_i16("s/v", -32767);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_i16_r(-32767), NULL, NULL));

    expect_pub_i32("s/v", -2147483647);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_i32_r(-2147483647), NULL, NULL));

    expect_pub_i64("s/v", -9223372036854775807LL);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_i64_r(-9223372036854775807LL), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_float(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/v", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));

    expect_pub_f32("s/v", 2.25f);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_f32_r(2.25f), NULL, NULL));

    expect_pub_f64("s/v", 2.25);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v", &fbp_union_f64_r(2.25), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_u32(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));
    fbp_pubsub_process(ps);

    // subscribe to parent, get retained value
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));

    // publish
    expect_pub_u32("s/hello/u32", 7);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(7), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_u32_retain_dedup(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));

    // Publish retained value
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));

    // Publish same retained value
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));

    // Publish different value
    expect_pub_u32("s/hello/u32", 99);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(99), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_u32_unretained(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));

    // Publish retained value
    expect_pub_u32("s/hello/u32", 0);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32(0), NULL, NULL));

    // Publish same retained value
    expect_pub_u32("s/hello/u32", 0);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32(0), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_subscribe_first(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));

    // publish
    expect_pub_u32("s/hello/u32", 42);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void on_publish(void * user_data) {
    (void) user_data;
    function_called();
}

static void test_on_publish_cbk(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);

    // publish
    fbp_pubsub_register_on_publish(ps, on_publish, NULL);
    expect_function_call(on_publish);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_retained_value_query_fn(void ** state) {
    (void) state;
    struct fbp_union_s value;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_not_equal(0, fbp_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));
    fbp_pubsub_process(ps);
    assert_int_equal(0, fbp_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(42, value.value.u32);
    fbp_pubsub_finalize(ps);
}

static void test_retained_value_query_req(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_QUERY_RSP, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/nope/u32", &fbp_union_u32(42), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));
    expect_pub_u32("s/hello/u32?", 42);
    assert_int_equal(0, fbp_pubsub_publish(ps, "?", &fbp_union_null(), NULL, NULL));
    expect_pub_u32("s/hello/u32?", 42);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/?", &fbp_union_null(), NULL, NULL));

    // and forward responses
    assert_int_equal(0, fbp_pubsub_publish(ps, "z/good?", &fbp_union_u32_r(99), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_retained_value_query_req_fwd(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_QUERY_REQ, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/?", &fbp_union_null(), NULL, NULL));
    expect_pub_null("?");
    assert_int_equal(0, fbp_pubsub_publish(ps, "?", &fbp_union_null(), NULL, NULL));
    expect_pub_null("r/?");
    assert_int_equal(0, fbp_pubsub_publish(ps, "r/?", &fbp_union_null(), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_do_not_update_same(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), on_pub, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_unsubscribe(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    fbp_pubsub_process(ps);
    assert_int_equal(0, fbp_pubsub_unsubscribe(ps, "s/hello", on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_unsubscribe_from_all(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/v1", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/v2", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    fbp_pubsub_process(ps);
    assert_int_equal(0, fbp_pubsub_unsubscribe_from_all(ps, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v1", &fbp_union_u32_r(42), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v2", &fbp_union_u32_r(43), NULL, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_unretained(void ** state) {
    (void) state;
    struct fbp_union_s value;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_not_equal(0, fbp_pubsub_query(ps, "s/hello/u32", &value));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32(42), NULL, NULL));
    fbp_pubsub_process(ps);
    assert_int_not_equal(0, fbp_pubsub_query(ps, "s/hello/u32", &value));

    // no callback, since not retained.
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_nopub(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", 0, on_pub, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32(42), NULL, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_meta_when_not_req_or_rsp_subscriber(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    fbp_pubsub_process(ps);
    // the subscriber should not receive the request or responses
    assert_int_equal(0, fbp_pubsub_publish(ps, "$", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/$", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v1$", &fbp_union_cjson_r(META1), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/$", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/v1$", &fbp_union_cjson_r(META1), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "?", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/?", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/?", &fbp_union_null(), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v1#", &fbp_union_cstr("1 msg1"), NULL, NULL));
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/v1#", &fbp_union_cstr("1 msg2"), NULL, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

#define META_REQ_PRE() \
    (void) state; \
    struct fbp_pubsub_s *ps = fbp_pubsub_initialize("s", 0); \
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_METADATA_REQ, on_pub, NULL)); \
    fbp_pubsub_process(ps)

#define META_REQ_POST() fbp_pubsub_finalize(ps)

static void test_meta_req_forward_root(void ** state) {
    META_REQ_PRE();
    expect_pub_null("$");   // expect forwarded root metadata request
    assert_int_equal(0, fbp_pubsub_publish(ps, "$", &fbp_union_null(), NULL, NULL));
    fbp_pubsub_process(ps);
    META_REQ_POST();
}

static void test_meta_req_forward_unowned(void ** state) {
    META_REQ_PRE();
    expect_pub_null("h/k/$");
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/k/$", &fbp_union_null(), NULL, NULL));
    fbp_pubsub_process(ps);
    META_REQ_POST();
}

static void test_meta_req_no_forward_owned(void ** state) {
    META_REQ_PRE();
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/$", &fbp_union_null(), NULL, NULL));
    fbp_pubsub_process(ps);
    META_REQ_POST();
}

static void test_meta_rsp_subscriber_root(void ** state) {
    (void) state;
    struct fbp_pubsub_s *ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_METADATA_RSP, on_pub, NULL));
    fbp_pubsub_process(ps);
    expect_pub_json("h/v2$", META1);  // expect the response, local or otherwise
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/v2$", &fbp_union_cjson_r(META1), NULL, NULL));
    fbp_pubsub_process(ps);
    fbp_pubsub_finalize(ps);
}

static void test_meta_rsp_subscriber_root_retained(void ** state) {
    (void) state;
    struct fbp_pubsub_s *ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/v2$", &fbp_union_cjson_r(META1), NULL, NULL));
    fbp_pubsub_process(ps);

    expect_pub_cstr(FBP_PUBSUB_TOPIC_PREFIX, "s");
    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s");
    // metadata is not forwarded on retained subscription, need explicit query
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_METADATA_RSP | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));
    fbp_pubsub_process(ps);

    // but do expect it on query
    expect_pub_json("s/v2$", META1);
    assert_int_equal(0, fbp_pubsub_publish(ps, "$", &fbp_union_null(), NULL, NULL));
    fbp_pubsub_process(ps);

    // even if we are the one requesting
    expect_pub_json("s/v2$", META1);
    assert_int_equal(0, fbp_pubsub_publish(ps, "$", &fbp_union_null(), on_pub, NULL));
    fbp_pubsub_process(ps);

    // unless it is unowned
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/$", &fbp_union_null(), on_pub, NULL));
    fbp_pubsub_process(ps);

    fbp_pubsub_finalize(ps);
}

static uint8_t on_publish_return_error(
        void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) user_data;
    uint8_t type = value->type;
    uint32_t value_u32 = value->value.u32;
    check_expected_ptr(topic);
    check_expected(type);
    check_expected(value_u32);
    return 1;
}

#define expect_epub_u32(topic_str, value) \
    expect_string(on_publish_return_error, topic, topic_str); \
    expect_value(on_publish_return_error, type, FBP_UNION_U32); \
    expect_value(on_publish_return_error, value_u32, value)

static void test_error(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/u32", FBP_PUBSUB_SFLAG_PUB, on_publish_return_error, NULL));
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_RETURN_CODE, on_pub, NULL));

    expect_epub_u32("s/hello/u32", 42);  // subscriber, returns error
    expect_pub_i32("s/hello/u32#", 1);   // forward to all meta response subscribers
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32", &fbp_union_u32_r(42), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_error_rsp_forward(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_RETURN_CODE, on_pub, NULL));
    fbp_pubsub_process(ps);

    // forward from our pubsub instance (strange, but allowed)
    expect_pub_i32("s/hello/u32#", 1);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/u32#", &fbp_union_i32(1), NULL, NULL));

    // forward from another pubsub instance
    expect_pub_i32("h/hello/u32#", 2);
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/hello/u32#", &fbp_union_i32(2), NULL, NULL));

    // unless the subscriber generated it.
    assert_int_equal(0, fbp_pubsub_publish(ps, "h/hello/u32#", &fbp_union_i32(3), on_pub, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_topic_prefix(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 0);
    expect_pub_cstr("_/topic/prefix", "s");
    assert_int_equal(0, fbp_pubsub_subscribe(ps, FBP_PUBSUB_TOPIC_PREFIX, FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));
    fbp_pubsub_finalize(ps);
}

#define SEP FBP_PUBSUB_UNIT_SEP_STR

static void test_topic_list(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 100);
    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s");
    assert_int_equal(0, fbp_pubsub_subscribe(ps, FBP_PUBSUB_TOPIC_LIST, FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETAIN, on_pub, NULL));

    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s" SEP "a");
    assert_int_equal(0, fbp_pubsub_publish(ps, FBP_PUBSUB_TOPIC_ADD, &fbp_union_str("a"), NULL, NULL));

    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s" SEP "a" SEP "b");
    assert_int_equal(0, fbp_pubsub_publish(ps, FBP_PUBSUB_TOPIC_ADD, &fbp_union_str("b"), NULL, NULL));

    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s" SEP "b");
    assert_int_equal(0, fbp_pubsub_publish(ps, FBP_PUBSUB_TOPIC_REMOVE, &fbp_union_str("a"), NULL, NULL));

    expect_pub_cstr(FBP_PUBSUB_TOPIC_LIST, "s");
    assert_int_equal(0, fbp_pubsub_publish(ps, FBP_PUBSUB_TOPIC_REMOVE, &fbp_union_str("b"), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

static void test_meta(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 100);

    assert_int_equal(0, fbp_pubsub_subscribe(ps, "s/hello/world", FBP_PUBSUB_SFLAG_PUB, on_pub, NULL));
    expect_pub_u8("s/hello/world", 2);
    assert_int_equal(0, fbp_pubsub_meta(ps, "s/hello/world", META2));

    expect_pub_u8("s/hello/world", 1);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("one"), NULL, NULL));

    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_RETURN_CODE, on_pub, NULL));
    expect_pub_i32("s/hello/world#", FBP_ERROR_PARAMETER_INVALID);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("__invalid__"), NULL, NULL));
    fbp_pubsub_finalize(ps);
}

static void test_meta_return_code(void ** state) {
    (void) state;
    struct fbp_pubsub_s * ps = fbp_pubsub_initialize("s", 100);

    assert_int_equal(0, fbp_pubsub_subscribe(ps, "", FBP_PUBSUB_SFLAG_PUB | FBP_PUBSUB_SFLAG_RETURN_CODE, on_pub, NULL));
    expect_pub_u32(FBP_PUBSUB_CONFIG_RETURN_CODE, 1);
    assert_int_equal(0, fbp_pubsub_publish(ps, FBP_PUBSUB_CONFIG_RETURN_CODE, &fbp_union_u32_r(1), NULL, NULL));
    expect_pub_u8("s/hello/world", 2);
    expect_pub_i32("s/hello/world#", 0);
    assert_int_equal(0, fbp_pubsub_meta(ps, "s/hello/world", META2));

    expect_pub_u8("s/hello/world", 1);
    expect_pub_i32("s/hello/world#", 0);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("one"), NULL, NULL));

    expect_pub_i32("s/hello/world#", FBP_ERROR_PARAMETER_INVALID);
    assert_int_equal(0, fbp_pubsub_publish(ps, "s/hello/world", &fbp_union_cstr_r("__invalid__"), NULL, NULL));

    fbp_pubsub_finalize(ps);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initialize, setup, teardown),
            cmocka_unit_test_setup_teardown(test_cstr, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str_but_too_big, setup, teardown),
            cmocka_unit_test_setup_teardown(test_str_full_buffer, setup, teardown),
            cmocka_unit_test_setup_teardown(test_integers, setup, teardown),
            cmocka_unit_test_setup_teardown(test_float, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32_retain_dedup, setup, teardown),
            cmocka_unit_test_setup_teardown(test_u32_unretained, setup, teardown),
            cmocka_unit_test_setup_teardown(test_subscribe_first, setup, teardown),
            cmocka_unit_test_setup_teardown(test_on_publish_cbk, setup, teardown),
            cmocka_unit_test_setup_teardown(test_retained_value_query_fn, setup, teardown),
            cmocka_unit_test_setup_teardown(test_retained_value_query_req, setup, teardown),
            cmocka_unit_test_setup_teardown(test_retained_value_query_req_fwd, setup, teardown),
            cmocka_unit_test_setup_teardown(test_do_not_update_same, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unsubscribe, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unsubscribe_from_all, setup, teardown),
            cmocka_unit_test_setup_teardown(test_unretained, setup, teardown),
            cmocka_unit_test_setup_teardown(test_nopub, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_when_not_req_or_rsp_subscriber, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_req_forward_root, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_req_forward_unowned, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_req_no_forward_owned, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_rsp_subscriber_root, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_rsp_subscriber_root_retained, setup, teardown),
            cmocka_unit_test_setup_teardown(test_error, setup, teardown),
            cmocka_unit_test_setup_teardown(test_error_rsp_forward, setup, teardown),
            cmocka_unit_test_setup_teardown(test_topic_prefix, setup, teardown),
            cmocka_unit_test_setup_teardown(test_topic_list, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta, setup, teardown),
            cmocka_unit_test_setup_teardown(test_meta_return_code, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
