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
#include "fitterbap/topic_list.h"
#include "fitterbap/ec.h"
#include <string.h>


#define SETUP() \
    (void) state; \
    struct fbp_topic_list_s l = FBP_TOPIC_LIST_INIT


#define assert_eq(str, l) \
    assert_string_equal((str), (l)->topic_list);


static void test_append(void **state) {
    SETUP();
    fbp_topic_list_append(&l, "hello");
    assert_eq("hello", &l);
    fbp_topic_list_append(&l, "there/");
    assert_eq("hello\x1fthere", &l);
    fbp_topic_list_append(&l, "world/");
    assert_eq("hello\x1fthere\x1fworld", &l);
    fbp_topic_list_append(&l, "goodness");
}

static void test_append_until_full(void ** state) {
    SETUP();
    char topic[] = "aweather";
    for (int i = 0; i < ((FBP_TOPIC_LIST_LENGTH_MAX + 8)/ 9 - 1); ++i) {
        topic[0] = 'a' + i;
        fbp_topic_list_append(&l, topic);
    }
    expect_assert_failure(fbp_topic_list_append(&l, "goodness"));
}

static void test_remove(void ** state) {
    SETUP();
    fbp_topic_list_append(&l, "x");
    fbp_topic_list_append(&l, "y");
    fbp_topic_list_append(&l, "z");
    assert_eq("x\x1fy\x1fz", &l);
    fbp_topic_list_remove(&l, "y");
    assert_eq("x\x1fz", &l);
}

static void test_remove_multiple(void ** state) {
    SETUP();
    l = ((struct fbp_topic_list_s) {.topic_list="x\x1fy\x1fx\x1fz"});
    fbp_topic_list_remove(&l, "x");
    assert_eq("y\x1fz", &l);
}

static void test_clear(void ** state) {
    SETUP();
    fbp_topic_list_append(&l, "x");
    fbp_topic_list_clear(&l);
    assert_eq("", &l);
}

int32_t iter_cbk(void * user_data, const char * topic) {
    (void) user_data;
    check_expected_ptr(topic);
    return mock_type(int32_t);
}

#define expect_iter(topic_, rv_) \
    expect_string(iter_cbk, topic, topic_); \
    will_return(iter_cbk, rv_)


static void test_iterate(void ** state) {
    SETUP();
    l = ((struct fbp_topic_list_s) {.topic_list="x\x1fy\x1fz"});
    expect_iter("x", 0);
    expect_iter("y", 0);
    expect_iter("z", 0);
    fbp_topic_list_iterate(&l, iter_cbk, NULL);
    assert_eq("x\x1fy\x1fz", &l);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_append),
            cmocka_unit_test(test_append_until_full),
            cmocka_unit_test(test_remove),
            cmocka_unit_test(test_remove_multiple),
            cmocka_unit_test(test_clear),
            cmocka_unit_test(test_iterate),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
