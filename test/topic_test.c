/*
 * Copyright 2021 Jetperch LLC
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
#include "fitterbap/topic.h"
#include "fitterbap/ec.h"
#include <string.h>


#define SETUP() \
    (void) state; \
    struct fbp_topic_s t = FBP_TOPIC_INIT


#define assert_topic(str, t) \
    assert_string_equal((str), (t)->topic); \
    assert_int_equal(strlen(str), (t)->length)


static void test_append(void **state) {
    SETUP();
    assert_int_equal(0, t.length);
    fbp_topic_append(&t, "hello");
    assert_topic("hello", &t);
    fbp_topic_append(&t, "there/");
    assert_topic("hello/there/", &t);
    fbp_topic_append(&t, "world");
    assert_topic("hello/there/world", &t);

    fbp_topic_append(&t, "message");
    expect_assert_failure(fbp_topic_append(&t, "too_much"));
}

static void test_reset(void **state) {
    SETUP();
    fbp_topic_append(&t, "hello");
    fbp_topic_reset(&t);
    assert_topic("", &t);
}

static void test_truncate(void **state) {
    SETUP();
    fbp_topic_append(&t, "hello");
    uint8_t length = t.length;
    fbp_topic_append(&t, "world");
    fbp_topic_truncate(&t, length);
    assert_topic("hello", &t);
}

static void test_set(void **state) {
    SETUP();
    fbp_topic_set(&t, "hello");
    assert_topic("hello", &t);
    expect_assert_failure(fbp_topic_set(&t, "hello/this/message/is/far/too/long/for/us"));
}

static void test_append_char(void **state) {
    SETUP();
    fbp_topic_append_char(&t, '#');
    assert_topic("#", &t);

    fbp_topic_set(&t, "hello");
    fbp_topic_append_char(&t, '#');
    assert_topic("hello#", &t);

    fbp_topic_set(&t, "hello/");
    fbp_topic_append_char(&t, '#');
    assert_topic("hello/#", &t);

    fbp_topic_set(&t, "01234567/01234567/01234567/012");
    fbp_topic_append_char(&t, '#');
    assert_topic("01234567/01234567/01234567/012#", &t);

    fbp_topic_set(&t, "01234567/01234567/01234567/0123");
    expect_assert_failure(fbp_topic_append_char(&t, '#'));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_append),
            cmocka_unit_test(test_reset),
            cmocka_unit_test(test_truncate),
            cmocka_unit_test(test_set),
            cmocka_unit_test(test_append_char),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
