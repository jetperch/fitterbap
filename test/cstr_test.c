/*
 * Copyright 2015-2021 Jetperch LLC
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
#include "fitterbap/cstr.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"


static void to_u32_empty(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(1, fbp_cstr_to_u32(0, &value));
    assert_int_equal(1, fbp_cstr_to_u32("", &value));
}

static void to_u32_zero(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, fbp_cstr_to_u32("0", &value));
    assert_int_equal(0, value);
    assert_int_equal(0, fbp_cstr_to_u32("  0  ", &value));
    assert_int_equal(0, value);
    assert_int_equal(1, fbp_cstr_to_u32("0", 0));
}

static void to_u32_42(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, fbp_cstr_to_u32("42", &value));
    assert_int_equal(42, value);
    assert_int_equal(0, fbp_cstr_to_u32("  42  ", &value));
    assert_int_equal(42, value);
}

static void to_u32_0_h(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(1, fbp_cstr_to_u32(" 0 h", &value));
}

static void to_u32_hex(void **state) {
    (void) state; /* unused */
    uint32_t value = 0;
    assert_int_equal(0, fbp_cstr_to_u32("0x12345678", &value));
    assert_int_equal(0x12345678, value);
}

struct i32s_case_s {
    const char * str;
    int32_t exponent;
    int32_t value;
};

static void to_i32s(void **state) {
    (void) state; /* unused */
    const struct i32s_case_s c[] = {
        {"1", 0, 1},
        {"1", 2, 100},
        {"1.01", 2, 101},
        {"   1.01   ", 2, 101},
        {"  +1.01  ", 2, 101},
        {"  -1.01   ", 2, -101},
        {"  1.010101   ", 2, 101},
    };

    for (fbp_size_t i = 0; i < FBP_ARRAY_SIZE(c); ++i) {
        int32_t x = 0;
        assert_int_equal(0, fbp_cstr_to_i32s(c[i].str, c[i].exponent, &x));
        assert_int_equal(c[i].value, x);
    }
}

const char MSG1[] = "hello world!";

static void copy_zero_tgt_size(void ** state) {
    (void) state;
    char tgt[] = "hello world";
    assert_int_equal(-1, fbp_cstr_copy(tgt,  MSG1, 0));
    assert_int_equal(tgt[0], 'h'); // not really meaningful
}

static void copy_zero_src_size(void ** state) {
    (void) state;
    char tgt[] = "hello world";
    assert_int_equal(0, fbp_cstr_copy(tgt, "", sizeof(tgt)));
    assert_int_equal(tgt[0], 0);
}

static void copy_normal(void ** state) {
    (void) state;
    char tgt[32];
    assert_int_equal(0, fbp_cstr_copy(tgt, MSG1, sizeof(tgt)));
    assert_string_equal(tgt, MSG1);
}

static void truncated(void ** state) {
    (void) state;
    char tgt[8];
    assert_int_equal(1, fbp_cstr_copy(tgt, MSG1, sizeof(tgt)));
    assert_string_equal(tgt, "hello w");
}

static void test_join_invalid_args(void ** state) {
    (void) state;
    char tgt[32];
    assert_int_equal(-1, fbp_cstr_join(NULL, "hello", " world", sizeof(tgt)));
    assert_int_equal(-1, fbp_cstr_join(tgt, "hello", " world", 0));
}

static void test_join(void ** state) {
    (void) state;
    char tgt[32];
    assert_int_equal(0, fbp_cstr_join(tgt, "hello", " world", sizeof(tgt)));
    assert_string_equal(tgt, "hello world");

    assert_int_equal(0, fbp_cstr_join(tgt, NULL, "world", sizeof(tgt)));
    assert_string_equal(tgt, "world");

    assert_int_equal(0, fbp_cstr_join(tgt, "", "world", sizeof(tgt)));
    assert_string_equal(tgt, "world");

    assert_int_equal(0, fbp_cstr_join(tgt, "hello", NULL, sizeof(tgt)));
    assert_string_equal(tgt, "hello");

    assert_int_equal(0, fbp_cstr_join(tgt, "hello", "", sizeof(tgt)));
    assert_string_equal(tgt, "hello");
}

static void test_join_trucate(void ** state) {
    (void) state;
    char tgt[8];
    assert_int_equal(1, fbp_cstr_join(tgt, "hello there", " world", sizeof(tgt)));
    assert_string_equal(tgt, "hello t");

    assert_int_equal(1, fbp_cstr_join(tgt, "hello", " world", sizeof(tgt)));
    assert_string_equal(tgt, "hello w");
}

static void test_join_reuse_src1(void ** state) {
    (void) state;
    char src1[16] = "hello";
    char src2[16] = " world";
    assert_int_equal(0, fbp_cstr_join(src1, src1, src2, sizeof(src1)));
    assert_string_equal(src1, "hello world");
}

static void test_join_reuse_src2(void ** state) {
    (void) state;
    char src1[16] = "hello";
    char src2[16] = " world";
    // not allowed
    assert_int_equal(2, fbp_cstr_join(src2, src1, src2, sizeof(src1)));
}

static void array_copy_truncated(void ** state) {
    (void) state;
    char tgt[8];
    assert_int_equal(1, fbp_cstr_array_copy(tgt, MSG1));
    assert_string_equal(tgt, "hello w");
}

static void atoi_good_with_space(void ** state) {
    (void) state;
    int32_t value = 3;
    assert_int_equal(0, fbp_cstr_to_i32("  42  ", &value));
    assert_int_equal(value, 42);
}

static void atoi_bad(void ** state) {
    (void) state;
    int32_t value = 3;
    assert_int_equal(1, fbp_cstr_to_i32("  hello  ", &value));
    assert_int_equal(value, 3);
}

static void atoi_invalid_params(void ** state) {
    (void) state;
    int value = 3;
    assert_int_equal(1, fbp_cstr_to_i32(NULL, &value));
    assert_int_equal(1, fbp_cstr_to_i32("42", NULL));
    assert_int_equal(1, fbp_cstr_to_i32(" ", &value));
    assert_int_equal(value, 3);
}

#if FBP_CONFIG_USE_CSTR_FLOAT
static void atof_good_with_space(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(0, fbp_cstr_to_f32("  4.2  ", &value));
    assert_float_equal(value, 4.2, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  123.456  ", &value));
    assert_float_equal(value, 123.456, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  +4.2  ", &value));
    assert_float_equal(value, 4.2, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  -4.2  ", &value));
    assert_float_equal(value, -4.2, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  -4.2f  ", &value));
    assert_float_equal(value, -4.2, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  -4.2F  ", &value));
    assert_float_equal(value, -4.2, 0.00001);
}

static void atof_good_exponent_form(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e0f  ", &value));
    assert_float_equal(value, 42.123, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  +42.123E1F  ", &value));
    assert_float_equal(value, 421.23, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e2  ", &value));
    assert_float_equal(value, 4212.3, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e4  ", &value));
    assert_float_equal(value, 42.123e4f, 1e0);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e8  ", &value));
    assert_float_equal(value, 42.123e8f, 0.1e4);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e16  ", &value));
    assert_float_equal(value, 42.123e16f, 0.1e12);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e32  ", &value));
    assert_float_equal(value, 42.123e32f, 0.1e28);
    assert_int_equal(0, fbp_cstr_to_f32("  -42.123e+2  ", &value));
    assert_float_equal(value, -4212.3, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  -42.123e-1f  ", &value));
    assert_float_equal(value, -4.2123, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  -42.123e-2F  ", &value));
    assert_float_equal(value, -0.42123, 0.00001);
    assert_int_equal(0, fbp_cstr_to_f32("  42.123e-2F  ", &value));
    assert_float_equal(value, 0.42123, 0.00001);
}

static void atof_bad(void ** state) {
    (void) state;
    float value = 3.0;
    assert_int_equal(1, fbp_cstr_to_f32("  hello ", &value));
    assert_int_equal(value, 3.0);
}
#endif

static void test_u32_to_cstr(void ** state) {
    (void) state;
    char str[12];
    assert_int_equal(0, fbp_u32_to_cstr(0, str, sizeof(str)));
    assert_string_equal("0", str);
    assert_int_equal(0, fbp_u32_to_cstr(1, str, sizeof(str)));
    assert_string_equal("1", str);
    assert_int_equal(0, fbp_u32_to_cstr(123456789, str, sizeof(str)));
    assert_string_equal("123456789", str);
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_u32_to_cstr(123456789, NULL, sizeof(str)));
    assert_int_equal(FBP_ERROR_PARAMETER_INVALID, fbp_u32_to_cstr(123456789, str, 0));
    assert_int_equal(FBP_ERROR_TOO_SMALL, fbp_u32_to_cstr(123456789, str, 2));
    assert_string_equal("", str);
}

static void test_toupper(void ** state) {
    (void) state;
    char msg1[] = "";
    char msg2[] = "lower UPPER 123%#";
    assert_int_equal(1, fbp_cstr_toupper(NULL));
    assert_int_equal(0, fbp_cstr_toupper(msg1));
    assert_string_equal("", msg1);
    assert_int_equal(0, fbp_cstr_toupper(msg2));
    assert_string_equal("LOWER UPPER 123%#", msg2);
}

static const char * const trueTable[] = {"v0", "v1", "v2", "v3", NULL};

static void to_index(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(0, fbp_cstr_to_index("v0", trueTable, &index));
    assert_int_equal(0, index);
    assert_int_equal(0, fbp_cstr_to_index("v1", trueTable, &index));
    assert_int_equal(1, index);
    assert_int_equal(0, fbp_cstr_to_index("v2", trueTable, &index));
    assert_int_equal(2, index);
    assert_int_equal(0, fbp_cstr_to_index("v3", trueTable, &index));
    assert_int_equal(3, index);
    assert_int_equal(1, fbp_cstr_to_index("other", trueTable, &index));
}

static void to_index_caps(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(1, fbp_cstr_to_index("V0", trueTable, &index));
}

static void to_index_invalid(void ** state) {
    (void) state;
    int index = 0;
    assert_int_equal(2, fbp_cstr_to_index("v0", trueTable, NULL));
    assert_int_equal(2, fbp_cstr_to_index("v0", NULL, &index));
    assert_int_equal(2, fbp_cstr_to_index(NULL, trueTable, &index));
}

static void to_bool(void ** state) {
    (void) state;
    bool value = false;
    assert_int_equal(0, fbp_cstr_to_bool("TRUE", &value));      assert_true(value);
    assert_int_equal(0, fbp_cstr_to_bool("true", &value));      assert_true(value);
    assert_int_equal(0, fbp_cstr_to_bool("on", &value));        assert_true(value);
    assert_int_equal(0, fbp_cstr_to_bool("1", &value));         assert_true(value);
    assert_int_equal(0, fbp_cstr_to_bool("enable", &value));    assert_true(value);

    assert_int_equal(0, fbp_cstr_to_bool("FALSE", &value));     assert_false(value);
    assert_int_equal(0, fbp_cstr_to_bool("false", &value));     assert_false(value);
    assert_int_equal(0, fbp_cstr_to_bool("off", &value));       assert_false(value);
    assert_int_equal(0, fbp_cstr_to_bool("0", &value));         assert_false(value);
    assert_int_equal(0, fbp_cstr_to_bool("disable", &value));   assert_false(value);

    assert_int_equal(1, fbp_cstr_to_bool("other", &value));
}

static void to_bool_invalid(void ** state) {
    (void) state;
    bool value = false;
    assert_int_equal(1, fbp_cstr_to_bool(NULL, &value));
    assert_int_equal(1, fbp_cstr_to_bool("TRUE", NULL));
}

static void casecmp(void ** state) {
    (void) state;
    assert_int_equal(0, fbp_cstr_casecmp("aajaa", "aajaa"));
    assert_int_equal(0, fbp_cstr_casecmp("aajaa", "aaJaa"));
    assert_int_equal(-1, fbp_cstr_casecmp("aajaa", "aakaa"));
    assert_int_equal(1, fbp_cstr_casecmp("aajaa", "aahaa"));
    assert_int_equal(0, fbp_cstr_casecmp("hello", "HELLO"));
}

static void starts_with(void **state) {
    (void) state;
    const char * hello_world = "hello_world";
    assert_ptr_equal(hello_world + 6, fbp_cstr_starts_with(hello_world, "hello_"));
    assert_null(fbp_cstr_starts_with(hello_world, "world"));
    assert_null(fbp_cstr_starts_with(hello_world, "heLLo_"));
    assert_null(fbp_cstr_starts_with(0, "heLLo_"));
    assert_ptr_equal(hello_world, fbp_cstr_starts_with(hello_world, 0));
}

static void ends_with(void **state) {
    (void) state;
    const char * hello_world = "hello_world";
    assert_ptr_equal(hello_world + 5, fbp_cstr_ends_with(hello_world, "_world"));
    assert_null(fbp_cstr_ends_with(hello_world, "hello"));
    assert_null(fbp_cstr_ends_with(hello_world, "worLD"));
    assert_null(fbp_cstr_ends_with(0, "world"));
    assert_ptr_equal(hello_world, fbp_cstr_ends_with(hello_world, 0));
}

static void hex_chars(void ** state) {
    (void) state;
    char v_upper[] = "0123456789ABCDEF";
    char v_lower[] = "0123456789abcdef";

    for (int i = 0; i < 16; ++i) {
        assert_int_equal(i, fbp_cstr_hex_to_u4(v_upper[i]));
        assert_int_equal(i, fbp_cstr_hex_to_u4(v_lower[i]));
        assert_int_equal(v_upper[i], fbp_cstr_u4_to_hex((uint8_t) i));
    }
}

static void hex_chars_invalid(void ** state) {
    (void) state;
    assert_int_equal(0, fbp_cstr_hex_to_u4('~'));
    assert_int_equal('0', fbp_cstr_u4_to_hex(33));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(to_u32_empty),
        cmocka_unit_test(to_u32_zero),
        cmocka_unit_test(to_u32_42),
        cmocka_unit_test(to_u32_0_h),
        cmocka_unit_test(to_u32_hex),
        cmocka_unit_test(to_i32s),
        cmocka_unit_test(copy_zero_tgt_size),
        cmocka_unit_test(copy_zero_src_size),
        cmocka_unit_test(copy_normal),
        cmocka_unit_test(truncated),
        cmocka_unit_test(test_join_invalid_args),
        cmocka_unit_test(test_join),
        cmocka_unit_test(test_join_trucate),
        cmocka_unit_test(test_join_reuse_src1),
        cmocka_unit_test(test_join_reuse_src2),
        cmocka_unit_test(array_copy_truncated),
        cmocka_unit_test(atoi_good_with_space),
        cmocka_unit_test(atoi_bad),
        cmocka_unit_test(atoi_invalid_params),

#if FBP_CONFIG_USE_CSTR_FLOAT
        cmocka_unit_test(atof_good_with_space),
        cmocka_unit_test(atof_good_exponent_form),
        cmocka_unit_test(atof_bad),
#endif

        cmocka_unit_test(test_u32_to_cstr),
        cmocka_unit_test(test_toupper),
        cmocka_unit_test(to_index),
        cmocka_unit_test(to_index_caps),
        cmocka_unit_test(to_index_invalid),
        cmocka_unit_test(to_bool),
        cmocka_unit_test(to_bool_invalid),
        cmocka_unit_test(casecmp),
        cmocka_unit_test(starts_with),
        cmocka_unit_test(ends_with),
        cmocka_unit_test(hex_chars),
        cmocka_unit_test(hex_chars_invalid),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
