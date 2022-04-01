/*
 * Copyright 2022 Jetperch LLC
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
#include "fitterbap/pubsub_meta.h"
#include "fitterbap/ec.h"

#define cstr(_value) ((struct fbp_union_s){.type=FBP_UNION_STR, .op=0, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.str=_value}, .size=strlen(_value) + 1})

const char * META1 = "{"
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

const char * META_NO_DEFAULT = "{"
    "\"dtype\": \"u8\","
    "\"brief\": \"Number selection.\""
"}";

static void test_basic(void **state) {
    (void) state;
    struct fbp_union_s value = fbp_union_null();
    assert_int_equal(0, fbp_pubsub_meta_syntax_check(META1));

    assert_int_equal(0, fbp_pubsub_meta_default(META1, &value));
    assert_true(fbp_union_eq(&fbp_union_u8(2), &value));
}

static void test_value(void **state) {
    (void) state;
    struct fbp_union_s value;
    value = fbp_union_u8(3);
    assert_int_equal(0, fbp_pubsub_meta_value(META1, &value));
    assert_true(fbp_union_eq(&fbp_union_u8(3), &value));

    value = cstr("three");
    assert_int_equal(0, fbp_pubsub_meta_value(META1, &value));
    assert_true(fbp_union_eq(&fbp_union_u8(3), &value));

    value = cstr("3");
    assert_int_equal(0, fbp_pubsub_meta_value(META1, &value));
    assert_true(fbp_union_eq(&fbp_union_u8(3), &value));

    value = cstr("__invalid__");
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_pubsub_meta_value(META1, &value));
}

static void test_no_default(void **state) {
    (void) state;
    struct fbp_union_s value = fbp_union_null();
    assert_int_equal(0, fbp_pubsub_meta_syntax_check(META_NO_DEFAULT));
    assert_int_equal(0, fbp_pubsub_meta_default(META_NO_DEFAULT, &value));
    assert_int_equal(FBP_UNION_NULL, value.type);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_basic),
            cmocka_unit_test(test_value),
            cmocka_unit_test(test_no_default),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
