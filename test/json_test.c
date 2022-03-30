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
#include "fitterbap/json.h"
#include "fitterbap/ec.h"

#define cstr_sz(_value, _size) ((struct fbp_union_s){.type=FBP_UNION_STR, .op=0, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.str=_value}, .size=_size})

#define check_expected_ptr_value(parameter) \
    _check_expected(__func__, "value", __FILE__, __LINE__, \
                    cast_ptr_to_largest_integral_type(parameter))

#define check_expected_value(parameter) \
    _check_expected(__func__, "value", __FILE__, __LINE__, \
                    cast_to_largest_integral_type(parameter))

static int32_t on_token(void * user_data, const struct fbp_union_s * token) {
    (void) user_data;
    uint8_t type = token->type;
    uint8_t op = token->op;
    check_expected(type);
    check_expected(op);
    switch (type) {
        case FBP_UNION_NULL: break;
        case FBP_UNION_STR: {
            size_t len = token->size;
            check_expected(len);
            check_expected_ptr_value(token->value.str);
            break;
        }
        case FBP_UNION_I32: check_expected_value(token->value.i32); break;
        default:
            assert_true(false);
    }
    return 0;
}

#define expect_tk(token__) {                                                                        \
    const struct fbp_union_s * token_ = (token__);                                                  \
    expect_value(on_token, type, token_->type);                                                     \
    expect_value(on_token, op, token_->op);                                                         \
    switch (token_->type) {                                                                         \
        case FBP_UNION_NULL:  break;                                                                \
        case FBP_UNION_STR: {                                                                       \
            size_t len = strlen(token_->value.str);                                                 \
            expect_value(on_token, len, len);                                                       \
            expect_memory(on_token, value, token_->value.str, len);                                 \
            break;                                                                                  \
        }                                                                                           \
        case FBP_UNION_I32:  expect_value(on_token, value, token_->value.i32); break;  \
        default: assert_true(false); break;                                                         \
    }                                                                                               \
}

#define expect_delim(op__) expect_tk(&((struct fbp_union_s){.type=FBP_UNION_NULL, .op=op__, .flags=0, .app=0, .value={.u64=0}, .size=0}))
#define expect_object_start() expect_delim(FBP_JSON_OBJ_START)
#define expect_object_end() expect_delim(FBP_JSON_OBJ_END)
#define expect_array_start() expect_delim(FBP_JSON_ARRAY_START)
#define expect_array_end() expect_delim(FBP_JSON_ARRAY_END)
#define expect_key(value__) expect_tk(&((struct fbp_union_s){.type=FBP_UNION_STR, .op=FBP_JSON_KEY, .flags=0, .app=0, .value={.str=value__}, .size=strlen(value__)}))

static void test_empty(void **state) {
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_json_parse(NULL, on_token, *state));
    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse("", on_token, *state));
    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse("    ", on_token, *state));
    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse("  \r\n\t  ", on_token, *state));
}

static void test_value_string(void **state) {
    expect_tk(&fbp_union_cstr("hello"));
    assert_int_equal(0, fbp_json_parse("   \"hello\"   ", on_token, *state));
    expect_tk(&fbp_union_cstr("hello\\n"));
    assert_int_equal(0, fbp_json_parse("   \"hello\\n\"   ", on_token, *state));
}

static void test_value_i32(void **state) {
    expect_tk(&fbp_union_i32(0));
    assert_int_equal(0, fbp_json_parse("   0   ", on_token, *state));
    expect_tk(&fbp_union_i32(42));
    assert_int_equal(0, fbp_json_parse("  \n42\t   ", on_token, *state));
    expect_tk(&fbp_union_i32(-42));
    assert_int_equal(0, fbp_json_parse("  \n-42\t   ", on_token, *state));
}

static void test_value_literals(void **state) {
    expect_tk(&fbp_union_null());
    assert_int_equal(0, fbp_json_parse("null", on_token, *state));
    expect_tk(&fbp_union_null());
    assert_int_equal(0, fbp_json_parse("   null   ", on_token, *state));
    expect_tk(&fbp_union_i32(0));
    assert_int_equal(0, fbp_json_parse("   false   ", on_token, *state));
    expect_tk(&fbp_union_i32(1));
    assert_int_equal(0, fbp_json_parse("   true   ", on_token, *state));

    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse("goober", on_token, *state));
}

static void test_obj_empty(void **state) {
    expect_object_start();
    expect_object_end();
    assert_int_equal(0, fbp_json_parse("   {\r\n\t    \n}\n   ", on_token, *state));
}

static void test_obj_1(void **state) {
    expect_object_start();
    expect_key("hello");
    expect_tk(&fbp_union_cstr("world"));
    expect_object_end();
    assert_int_equal(0, fbp_json_parse("{ \"hello\": \"world\" }", on_token, *state));
}

static void test_obj_N(void **state) {
    expect_object_start();
    expect_key("hello");
    expect_tk(&fbp_union_cstr("world"));
    expect_key("json");
    expect_tk(&fbp_union_cstr("parse"));
    expect_object_end();
    assert_int_equal(0, fbp_json_parse("{ \"hello\":\"world\", \"json\" : \"parse\" }", on_token, *state));
}

static void test_obj_trailing_comma(void **state) {
    expect_object_start();
    expect_key("hello");
    expect_tk(&fbp_union_cstr("world"));
    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse("{ \"hello\":\"world\", }", on_token, *state));
}

static void test_array_1(void **state) {
    expect_array_start();
    expect_tk(&fbp_union_i32(1));
    expect_array_end();
    assert_int_equal(0, fbp_json_parse(" [ 1 ]", on_token, *state));
}

static void test_array_N(void **state) {
    expect_array_start();
    expect_tk(&fbp_union_i32(1));
    expect_tk(&fbp_union_i32(2));
    expect_tk(&fbp_union_i32(3));
    expect_tk(&fbp_union_cstr("apple"));
    expect_tk(&fbp_union_cstr("orange"));
    expect_array_end();
    assert_int_equal(0, fbp_json_parse(" [ 1, 2, 3, \"apple\", \"orange\" ]", on_token, *state));
}

static void test_array_trailing_comma(void **state) {
    expect_array_start();
    expect_tk(&fbp_union_i32(1));
    assert_int_equal(FBP_ERROR_SYNTAX_ERROR, fbp_json_parse(" [ 1, ]", on_token, *state));
}

static void test_strcmp(void **state) {
    assert_int_equal(-2, fbp_json_strcmp(NULL, &cstr_sz("b", 1)));
    assert_int_equal(-1, fbp_json_strcmp("", &cstr_sz("b", 1)));
    assert_int_equal(-1, fbp_json_strcmp("a", &cstr_sz("b", 1)));
    assert_int_equal(0, fbp_json_strcmp("b", &cstr_sz("b", 1)));
    assert_int_equal(1, fbp_json_strcmp("c", &cstr_sz("b", 1)));

    assert_int_equal(0, fbp_json_strcmp("hello", &cstr_sz("hello", 5)));
    assert_int_equal(-1, fbp_json_strcmp("hell", &cstr_sz("hello", 5)));
    assert_int_equal(1, fbp_json_strcmp("hello", &cstr_sz("hello", 4)));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_empty),
            cmocka_unit_test(test_value_string),
            cmocka_unit_test(test_value_i32),
            cmocka_unit_test(test_value_literals),
            cmocka_unit_test(test_obj_empty),
            cmocka_unit_test(test_obj_1),
            cmocka_unit_test(test_obj_N),
            cmocka_unit_test(test_obj_trailing_comma),
            cmocka_unit_test(test_array_1),
            cmocka_unit_test(test_array_N),
            cmocka_unit_test(test_array_trailing_comma),

            cmocka_unit_test(test_strcmp),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
