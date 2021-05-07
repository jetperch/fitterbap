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
#include "fitterbap/argchk.h"
#include "fitterbap.h"


#define F FBP_ARGCHK_FAIL_RETURN_CODE

#define FUNC(argchk_macro, arg_type) \
static int (fn_ ## argchk_macro) (arg_type x) { \
    FBP_ARGCHK_ ## argchk_macro(x); \
    return 0; \
}

FUNC(TRUE, int)
FUNC(FALSE, int)
FUNC(NOT_NULL, int *)
FUNC(GTE_ZERO, int)
FUNC(GT_ZERO, int)
FUNC(LTE_ZERO, int)
FUNC(LT_ZERO, int)

static int fn_RANGE_INT(int x) {
    FBP_ARGCHK_RANGE_INT(x, -10, 20);
    return 0;
}

static void test_true(void **state) {
    (void) state;
    assert_int_equal(0, fn_TRUE(1));
    assert_int_equal(F, fn_TRUE(0));
}

static void test_false(void **state) {
    (void) state;
    assert_int_equal(0, fn_FALSE(0));
    assert_int_equal(F, fn_FALSE(1));
}

static void test_not_null(void **state) {
    (void) state;
    int x = 0;
    assert_int_equal(0, fn_NOT_NULL(&x));
    assert_int_equal(F, fn_NOT_NULL(0));
}

static void test_gte_zero(void **state) {
    (void) state;
    assert_int_equal(0, fn_GTE_ZERO(0));
    assert_int_equal(0, fn_GTE_ZERO(100));
    assert_int_equal(F, fn_GTE_ZERO(-1));
}

static void test_gt_zero(void **state) {
    (void) state;
    assert_int_equal(0, fn_GT_ZERO(1));
    assert_int_equal(0, fn_GT_ZERO(100));
    assert_int_equal(F, fn_GT_ZERO(0));
}

static void test_lte_zero(void **state) {
    (void) state;
    assert_int_equal(0, fn_LTE_ZERO(0));
    assert_int_equal(0, fn_LTE_ZERO(-100));
    assert_int_equal(F, fn_LTE_ZERO(1));
}

static void test_lt_zero(void **state) {
    (void) state;
    assert_int_equal(0, fn_LT_ZERO(-1));
    assert_int_equal(0, fn_LT_ZERO(-100));
    assert_int_equal(F, fn_LT_ZERO(0));
}

static void test_range_int(void **state) {
    (void) state;
    assert_int_equal(0, fn_RANGE_INT(-10));
    assert_int_equal(0, fn_RANGE_INT(20));
    assert_int_equal(F, fn_RANGE_INT(-11));
    assert_int_equal(F, fn_RANGE_INT(21));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_true),
            cmocka_unit_test(test_false),
            cmocka_unit_test(test_not_null),
            cmocka_unit_test(test_gte_zero),
            cmocka_unit_test(test_gt_zero),
            cmocka_unit_test(test_lte_zero),
            cmocka_unit_test(test_lt_zero),
            cmocka_unit_test(test_range_int),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
