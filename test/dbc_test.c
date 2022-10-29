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
#include "fitterbap/dbc.h"
#include "fitterbap.h"


static void test_true(void **state) {
    (void) state;
    FBP_DBC_TRUE(1);
    expect_assert_failure(FBP_DBC_TRUE(0));
}

static void test_false(void **state) {
    (void) state;
    FBP_DBC_FALSE(0);
    expect_assert_failure(FBP_DBC_FALSE(1));
}

static void test_not_null(void **state) {
    (void) state;
    int x = 0;
    int * p = &x;
    FBP_DBC_NOT_NULL(p);
    expect_assert_failure(FBP_DBC_NOT_NULL(0));
}

static void test_equal(void **state) {
    (void) state;
    FBP_DBC_EQUAL(42, 42);
    expect_assert_failure(FBP_DBC_EQUAL(1, 2));
}

static void test_gte_zero(void **state) {
    (void) state;
    FBP_DBC_GTE_ZERO(0);
    FBP_DBC_GTE_ZERO(100);
    expect_assert_failure(FBP_DBC_GTE_ZERO(-1));
}

static void test_gt_zero(void **state) {
    (void) state;
    FBP_DBC_GT_ZERO(1);
    FBP_DBC_GT_ZERO(100);
    expect_assert_failure(FBP_DBC_GT_ZERO(0));
}

static void test_lte_zero(void **state) {
    (void) state;
    FBP_DBC_LTE_ZERO(0);
    FBP_DBC_LTE_ZERO(-100);
    expect_assert_failure(FBP_DBC_LTE_ZERO(1));
}

static void test_lt_zero(void **state) {
    (void) state;
    FBP_DBC_LT_ZERO(-1);
    FBP_DBC_LT_ZERO(-100);
    expect_assert_failure(FBP_DBC_LT_ZERO(0));
}

static void test_range_int(void **state) {
    (void) state;
    FBP_DBC_RANGE_INT(-10, -10, 20);
    FBP_DBC_RANGE_INT(20, -10, 20);
    expect_assert_failure(FBP_DBC_RANGE_INT(-11, -10, 20));
    expect_assert_failure(FBP_DBC_RANGE_INT(21, -10, 20));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_true),
            cmocka_unit_test(test_false),
            cmocka_unit_test(test_not_null),
            cmocka_unit_test(test_equal),
            cmocka_unit_test(test_gte_zero),
            cmocka_unit_test(test_gt_zero),
            cmocka_unit_test(test_lte_zero),
            cmocka_unit_test(test_lt_zero),
            cmocka_unit_test(test_range_int),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
