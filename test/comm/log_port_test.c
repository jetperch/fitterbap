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
#include "fitterbap/comm/log_port.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "tinyprintf.h"
#include "port_hal.inc"


int64_t utc_ = 0;

int64_t fbp_time_utc() {
    return utc_;
}

static void on_publish_check(void * user_data) {
    check_expected_ptr(user_data);
}

static void on_publish_nocheck(void * user_data) {
    (void) user_data;
}

static uint32_t msg_format(struct fbp_logp_msg_buffer_s * buf, int64_t timestamp, char origin_prefix, uint8_t level, const char * filename, uint16_t line, const char * msg) {
    int i;
    char * p;
    buf->header.version = FBP_LOGP_VERSION;
    buf->header.origin_prefix = origin_prefix;
    buf->header.line = line;
    buf->header.level = level;
    buf->header.origin_thread = 0;
    buf->header.rsv8_1 = 0;
    buf->header.rsv8_2 = 0;
    buf->header.timestamp = timestamp;
    p = buf->data;
    for (i = 0; (i < FBP_LOGP_FILENAME_SIZE_MAX) && (*filename); ++i) {
        *p++ = *filename++;
    }
    *p++ = FBP_LOGP_SEP;
    for (i = 0; (i < FBP_LOGP_FILENAME_SIZE_MAX) && (*msg); ++i) {
        *p++ = *msg++;
    }
    *p++ = 0;
    return (p - (char *) buf);
}

#define expect_msg(timestamp_, origin_prefix_, level_, filename_, line_, format_, ...) { \
    struct fbp_logp_msg_buffer_s buf_;                                       \
    uint32_t length_ = msg_format(&buf_, timestamp_, origin_prefix_, level_, filename_, line_, format_, __VA_ARGS__); \
    expect_send(2, 0, &buf_, length_, 0);                                     \
}

static void on_recv(void * user_data, struct fbp_logp_record_s const * record) {
    (void) user_data;
    uint64_t timestamp = record->timestamp;
    uint8_t level = record->level;
    uint8_t origin_prefix = record->origin_prefix;
    uint8_t origin_thread = record->origin_thread;
    uint32_t line = record->line;
    char * filename = record->filename;
    char * message = record->message;

    check_expected(timestamp);
    check_expected(level);
    check_expected(origin_prefix);
    check_expected(origin_thread);
    check_expected(line);
    check_expected_ptr(filename);
    check_expected_ptr(message);
}

#define expect_recv(timestamp_, level_, origin_prefix_, origin_thread_, line_, filename_, message_) \
    expect_value(on_recv, timestamp, timestamp_);                                                   \
    expect_value(on_recv, level, level_);                                                           \
    expect_value(on_recv, origin_prefix, origin_prefix_);                                           \
    expect_value(on_recv, origin_thread, origin_thread_);                                           \
    expect_value(on_recv, line, line_);                                                             \
    expect_string(on_recv, filename, filename_);                                                    \
    expect_string(on_recv, message, message_);

static struct fbp_port_api_s * initialize(bool check_publish) {
    struct fbp_logp_config_s config = {
            .msg_buffers_max = 4,
            .on_publish_fn = check_publish ? on_publish_check : on_publish_nocheck,
            .on_publish_user_data = NULL,
            .mutex = NULL,
    };

    struct fbp_port_api_s * api = fbp_logp_factory(&config);
    assert_int_equal(FBP_ERROR_UNAVAILABLE, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));

    api->topic_prefix = "a";
    api->port_id = 2;
    api->transport = (struct fbp_transport_s *) api;

    assert_int_equal(0, api->initialize(api));
    return api;
}

#define SETUP(check_publish)                                    \
    (void) state;                                               \
    utc_ = 0;                                                   \
    struct fbp_port_api_s * api = initialize(check_publish)

#define TEARDOWN() \
    assert_int_equal(0, api->finalize(api))


static void test_initialize(void ** state) {
    SETUP(true);
    TEARDOWN();
}

static void test_publish_one(void ** state) {
    SETUP(true);
    assert_int_equal(FBP_ERROR_UNAVAILABLE, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));

    expect_msg(0, 'a', FBP_LOG_LEVEL_INFO, "file.c", 10, "hello");
    fbp_logp_process(api);

    TEARDOWN();
}

static void test_publish_record(void ** state) {
    SETUP(true);
    struct fbp_logp_record_s record = {
            .origin_prefix = 'a',
            .line = 10,
            .level = FBP_LOG_LEVEL_INFO,
            .origin_thread = 0,
            .timestamp = 1000,
            .filename = "file.c",
            .message = "hello",
    };
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish_record(api, &record));

    expect_msg(1000, 'a', FBP_LOG_LEVEL_INFO, "file.c", 10, "hello");
    fbp_logp_process(api);

    TEARDOWN();
}

static void test_publish_until_full(void ** state) {
    SETUP(true);
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);

    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file1.c", 10, "%s", "hello"));
    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file2.c", 11, "%s", " there "));
    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file3.c", 12, "%d", 42));
    expect_value(on_publish_check, user_data, NULL);
    utc_ = 1234;
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file4.c", 13, "%s", " world"));
    assert_int_equal(FBP_ERROR_FULL, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "full"));

    expect_msg(0, 'a', FBP_LOG_LEVEL_INFO, "file1.c", 10, "hello");
    expect_msg(0, 'a', FBP_LOG_LEVEL_INFO, "file2.c", 11, " there ");
    expect_msg(0, 'a', FBP_LOG_LEVEL_INFO, "file3.c", 12, "42");
    expect_msg(1234, 'a', FBP_LOG_LEVEL_INFO, "file4.c", 13, " world");
    fbp_logp_process(api);

    TEARDOWN();
}

static void test_publish_with_transport_full(void ** state) {
    SETUP(true);
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    expect_value(on_publish_check, user_data, NULL);
    assert_int_equal(0, fbp_logp_publish(api, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));

    // Full on first try
    expect_send_error(FBP_ERROR_NOT_ENOUGH_MEMORY);
    assert_int_equal(FBP_ERROR_NOT_ENOUGH_MEMORY, fbp_logp_process(api));

    // But completes normally next try
    expect_msg(0, 'a', FBP_LOG_LEVEL_INFO, "file.c", 10, "hello");
    assert_int_equal(0, fbp_logp_process(api));

    TEARDOWN();
}

static void test_receive(void ** state) {
    SETUP(true);
    fbp_logp_recv_register(api, on_recv, api);
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    struct fbp_logp_msg_buffer_s buf;
    uint32_t length = msg_format(&buf, 1234, 'b', FBP_LOG_LEVEL_WARNING, "file.c", 42, "hi?");
    expect_recv(1234, FBP_LOG_LEVEL_WARNING, 'b', 0, 42, "file.c", "hi?");
    api->on_recv(api, 2, FBP_TRANSPORT_SEQ_SINGLE, 0, (uint8_t *) &buf, length);
    TEARDOWN();
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_initialize),
            cmocka_unit_test(test_publish_one),
            cmocka_unit_test(test_publish_record),
            cmocka_unit_test(test_publish_until_full),
            cmocka_unit_test(test_publish_with_transport_full),
            cmocka_unit_test(test_receive),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
