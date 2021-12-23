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
#include "fitterbap/comm/log_port.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/pubsub.h"
#include "tinyprintf.h"
#include "port_hal.inc"


#define MSG_SZ (sizeof(struct fbp_logh_header_s) + FBP_LOGP_DATA_SIZE_MAX)


#define HEADER(level_, origin_prefix_, origin_thread_, line_, timestamp_) \
    (struct fbp_logh_header_s) { \
        .version = FBP_LOGH_VERSION, \
        .level = FBP_LOG_LEVEL_##level_, \
        .origin_prefix = 'a', \
        .origin_thread = 0, \
        .line = 10, \
        .timestamp = 0, \
    }

static int32_t msg_format(uint8_t * buf, struct fbp_logh_header_s * header, const char * filename, const char * message) {
    char * p = (char *) (buf + sizeof(*header));
    memcpy(buf, header, sizeof(*header));
    while (*filename) {
        *p++ = *filename++;
    }
    *p++ = FBP_LOGP_SEP;
    while (*message) {
        *p++ = *message++;
    }
    *p++ = 0;
    int32_t sz = ((uint8_t *) p - buf);
    return sz;
}

#define expect_msg(header_, filename_, message_) {                  \
    uint8_t msg[MSG_SZ];                                            \
    int32_t sz = msg_format(msg, header_, filename_, message_);     \
    expect_send(2, 0, msg, sz);                                     \
}

static int32_t on_recv(void * user_data, const struct fbp_logh_header_s * header, const char * filename, const char * message) {
    (void) user_data;
    uint64_t timestamp = header->timestamp;
    uint8_t level = header->level;
    uint8_t origin_prefix = header->origin_prefix;
    uint8_t origin_thread = header->origin_thread;
    uint32_t line = header->line;

    check_expected(timestamp);
    check_expected(level);
    check_expected(origin_prefix);
    check_expected(origin_thread);
    check_expected(line);
    check_expected_ptr(filename);
    check_expected_ptr(message);
    return 0;
}

#define expect_recv(header_, filename_, message_)                                                   \
    expect_value(on_recv, timestamp, (header_)->timestamp);                                         \
    expect_value(on_recv, level, (header_)->level);                                                 \
    expect_value(on_recv, origin_prefix, (header_)->origin_prefix);                                 \
    expect_value(on_recv, origin_thread, (header_)->origin_thread);                                 \
    expect_value(on_recv, line, (header_)->line);                                                   \
    expect_string(on_recv, filename, filename_);                                                    \
    expect_string(on_recv, message, message_)

static struct fbp_port_api_s * initialize() {
    struct fbp_port_api_s * api = fbp_logp_factory();
    struct fbp_port_config_s config = {
        .transport = (struct fbp_transport_s *) api,
        .port_id = 2,
        .pubsub = (struct fbp_pubsub_s *) api,
        .topic_prefix = {.topic = "a/2", .length = 1},
        .evm = {0, 0, 0, 0}
    };
    expect_meta("a/2/level");
    expect_subscribe("a/2/level", 0);
    expect_publish_u8("a/2/level", FBP_LOGP_LEVEL);
    assert_int_equal(0, api->initialize(api, &config));

    return api;
}

#define SETUP()                                    \
    (void) state;                                  \
    struct fbp_port_api_s * api = initialize()

#define TEARDOWN() \
    assert_int_equal(0, api->finalize(api))


static void test_initialize(void ** state) {
    SETUP();
    TEARDOWN();
}

static void test_publish_one(void ** state) {
    SETUP();
    assert_int_equal(FBP_ERROR_UNAVAILABLE, fbp_logp_recv(api, &HEADER(CRITICAL, 'a', 0, 10, 0), "file.c", "hello"));
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    expect_msg(&HEADER(CRITICAL, 'a', 0, 10, 0), "file.c", "hello");
    assert_int_equal(0, fbp_logp_recv(api, &HEADER(CRITICAL, 'a', 0, 10, 0), "file.c", "hello"));
    TEARDOWN();
}

static void test_publish_filtered(void ** state) {
    SETUP();
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    assert_int_equal(0, fbp_logp_recv(api, &HEADER(DEBUG3, 'a', 0, 10, 0), "file.c", "hello"));
    TEARDOWN();
}

static void test_receive(void ** state) {
    SETUP();
    fbp_logp_handler_register(api, on_recv, api);
    api->on_event(api, FBP_DL_EV_APP_CONNECTED);
    uint8_t buf[MSG_SZ];
    uint32_t length = msg_format(buf, &HEADER(CRITICAL, 'a', 0, 10, 42), "file.c", "hi?");
    expect_recv(&HEADER(CRITICAL, 'a', 0, 10, 42), "file.c", "hi?");
    api->on_recv(api, 2, FBP_TRANSPORT_SEQ_SINGLE, 0, (uint8_t *) &buf, length);

    length = msg_format(buf, &HEADER(DEBUG3, 'a', 0, 10, 42), "file.c", "hi?");
    api->on_recv(api, 2, FBP_TRANSPORT_SEQ_SINGLE, 0, (uint8_t *) &buf, length);
    TEARDOWN();
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_initialize),
            cmocka_unit_test(test_publish_one),
            cmocka_unit_test(test_publish_filtered),
            cmocka_unit_test(test_receive),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
