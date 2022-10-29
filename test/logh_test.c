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
#include "fitterbap/logh.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "tinyprintf.h"


int64_t utc_ = 0;

int64_t test_time() {
    return utc_;
}

static void on_publish_check(void * user_data) {
    check_expected_ptr(user_data);
}

static int32_t on_dispatch(void * user_data, struct fbp_logh_header_s const * header,
                         const char * filename, const char * message) {
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
    return mock_type(int32_t);
}

#define expect_dispatch(timestamp_, level_, origin_prefix_, origin_thread_, filename_, line_, message_) \
    expect_value(on_dispatch, timestamp, timestamp_);                                                   \
    expect_value(on_dispatch, level, level_);                                                           \
    expect_value(on_dispatch, origin_prefix, origin_prefix_);                                           \
    expect_value(on_dispatch, origin_thread, origin_thread_);                                           \
    expect_value(on_dispatch, line, line_);                                                             \
    expect_string(on_dispatch, filename, filename_);                                                    \
    expect_string(on_dispatch, message, message_);                                                      \
    will_return(on_dispatch, 0)


#define expect_dispatch_return_error(ec_)       \
    expect_any(on_dispatch, timestamp);         \
    expect_any(on_dispatch, level);             \
    expect_any(on_dispatch, origin_prefix);     \
    expect_any(on_dispatch, origin_thread);     \
    expect_any(on_dispatch, line);              \
    expect_any(on_dispatch, filename);          \
    expect_any(on_dispatch, message);           \
    will_return(on_dispatch, ec_)

#define SETUP(check_publish)                                                \
    (void) state;                                                           \
    utc_ = 0;                                                               \
    struct fbp_logh_s * l = fbp_logh_initialize('a', 4, test_time);         \
    if (check_publish) {                                                    \
        fbp_logh_publish_register(l, on_publish_check, NULL);               \
    }                                                                       \
    assert_int_equal(0, fbp_logh_dispatch_register(l, on_dispatch, NULL))

#define TEARDOWN() \
    fbp_logh_finalize(l)


static void test_initialize(void ** state) {
    SETUP(true);
    TEARDOWN();
}

static void test_publish_one(void ** state) {
    SETUP(true);
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));

    expect_dispatch(utc_, FBP_LOG_LEVEL_INFO, 'a', 0, "file.c", 10, "hello");
    fbp_logh_process(l);

    TEARDOWN();
}

static void test_singleton(void ** state) {
    SETUP(true);
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(NULL, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));
    expect_dispatch(utc_, FBP_LOG_LEVEL_INFO, 'a', 0, "file.c", 10, "hello");
    fbp_logh_process(NULL);
    TEARDOWN();
}

static void test_publish_formatted(void ** state) {
    SETUP(true);
    struct fbp_logh_header_s header = {
            .version = FBP_LOGH_VERSION,
            .level = FBP_LOG_LEVEL_INFO,
            .origin_prefix = 'a',
            .origin_thread = 2,
            .line = 10,
            .timestamp = 1000,
    };
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish_formatted(l, &header, "file.c", "hello"));

    expect_dispatch(1000, FBP_LOG_LEVEL_INFO, 'a', 2, "file.c", 10, "hello");
    fbp_logh_process(l);

    TEARDOWN();
}

static void test_publish_until_full(void ** state) {
    SETUP(true);

    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file1.c", 10, "%s", "hello"));
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file2.c", 11, "%s", " there "));
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file3.c", 12, "%d", 42));
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    utc_ = 1234;
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file4.c", 13, "%s", " world"));
    assert_int_equal(FBP_ERROR_FULL, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "full"));

    expect_dispatch(0, FBP_LOG_LEVEL_INFO, 'a', 0, "file1.c", 10, "hello");
    expect_dispatch(0, FBP_LOG_LEVEL_INFO, 'a', 0, "file2.c", 11, " there ");
    expect_dispatch(0, FBP_LOG_LEVEL_INFO, 'a', 0, "file3.c", 12, "42");
    expect_dispatch(1234, FBP_LOG_LEVEL_INFO, 'a', 0, "file4.c", 13, " world");
    fbp_logh_process(l);

    TEARDOWN();
}

static void test_publish_with_receiver_full(void ** state) {
    SETUP(true);
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));

    // Full on first try
    expect_dispatch_return_error(FBP_ERROR_FULL);
    assert_int_equal(FBP_ERROR_FULL, fbp_logh_process(l));

    // But completes normally next try
    expect_dispatch(0, FBP_LOG_LEVEL_INFO, 'a', 0, "file.c", 10, "hello");
    assert_int_equal(0, fbp_logh_process(l));

    TEARDOWN();
}

static void test_dispatcher_unregister(void ** state) {
    SETUP(true);
    assert_int_equal(0, fbp_logh_dispatch_unregister(l, on_dispatch, NULL));
    assert_int_equal(FBP_ERROR_NOT_FOUND, fbp_logh_dispatch_unregister(l, on_dispatch, NULL));
    expect_value(on_publish_check, user_data, (intptr_t) NULL);
    assert_int_equal(0, fbp_logh_publish(l, FBP_LOG_LEVEL_INFO, "file.c", 10, "%s", "hello"));
    assert_int_equal(0, fbp_logh_process(l));
    TEARDOWN();
}

static void test_dispatcher_register_too_many(void ** state) {
    SETUP(false);
    assert_int_equal(0, fbp_logh_dispatch_register(l, on_dispatch, (void *) 4));
    assert_int_equal(0, fbp_logh_dispatch_register(l, on_dispatch, (void *) 8));
    assert_int_equal(0, fbp_logh_dispatch_register(l, on_dispatch, (void *) 12));
    assert_int_equal(FBP_ERROR_FULL, fbp_logh_dispatch_register(l, on_dispatch, (void *) 16));
    TEARDOWN();
}



int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_initialize),
            cmocka_unit_test(test_publish_one),
            cmocka_unit_test(test_singleton),
            cmocka_unit_test(test_publish_formatted),
            cmocka_unit_test(test_publish_until_full),
            cmocka_unit_test(test_publish_with_receiver_full),
            cmocka_unit_test(test_dispatcher_unregister),
            cmocka_unit_test(test_dispatcher_register_too_many),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
