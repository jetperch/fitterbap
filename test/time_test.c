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
#include "fitterbap/common_header.h"


#define ABS(x) ( (x) < 0 ? -x : x)
#define CLOSE(x, t) ( ABS(x) < (t) )

static void test_constants(void **state) {
    (void) state;
    assert_int_equal(1 << 30, FBP_TIME_SECOND);
    assert_int_equal((FBP_TIME_SECOND + 500) / 1000, FBP_TIME_MILLISECOND);
    assert_int_equal((FBP_TIME_SECOND + 500000) / 1000000, FBP_TIME_MICROSECOND);
    assert_int_equal(1, FBP_TIME_NANOSECOND);
    assert_int_equal(FBP_TIME_SECOND * 60, FBP_TIME_MINUTE);
    assert_int_equal(FBP_TIME_SECOND * 60 * 60, FBP_TIME_HOUR);
    assert_int_equal(FBP_TIME_SECOND * 60 * 60 * 24, FBP_TIME_DAY);
}

static void test_f64(void **state) {
    (void) state;
    assert_true(CLOSE(1.0 - FBP_TIME_TO_F64(FBP_TIME_SECOND), 1e-9));
    assert_true(FBP_TIME_SECOND == FBP_F64_TO_TIME(1.0));
    assert_true(CLOSE(0.001 - FBP_TIME_TO_F64(FBP_TIME_MILLISECOND), 1e-9));
    assert_true(FBP_TIME_MILLISECOND == FBP_F64_TO_TIME(0.001));
}

static void test_f32(void **state) {
    (void) state;
    assert_true(CLOSE(1.0f - FBP_TIME_TO_F32(FBP_TIME_SECOND), 1e-9));
    assert_true(FBP_TIME_SECOND == FBP_F32_TO_TIME(1.0f));
    assert_true(CLOSE(0.001f - FBP_TIME_TO_F32(FBP_TIME_MILLISECOND), 1e-9));
    assert_true(FBP_TIME_MILLISECOND == FBP_F32_TO_TIME(0.001f));
}

static void test_convert_time_to(void **state) {
    (void) state;
    assert_int_equal(1, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND));
    assert_int_equal(1, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND + 1));
    assert_int_equal(1, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND - 1));
    assert_int_equal(2, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND + FBP_TIME_SECOND / 2));
    assert_int_equal(1, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND - FBP_TIME_SECOND / 2));
    assert_int_equal(0, FBP_TIME_TO_SECONDS(FBP_TIME_SECOND - FBP_TIME_SECOND / 2 - 1));
    assert_int_equal(1000, FBP_TIME_TO_MILLISECONDS(FBP_TIME_SECOND));
    assert_int_equal(1000000, FBP_TIME_TO_MICROSECONDS(FBP_TIME_SECOND));
    assert_int_equal(1000000000, FBP_TIME_TO_NANOSECONDS(FBP_TIME_SECOND));
}

static void test_convert_to_time(void **state) {
    (void) state;
    assert_int_equal(FBP_TIME_SECOND, FBP_SECONDS_TO_TIME(1));
    assert_int_equal(FBP_TIME_SECOND, FBP_MILLISECONDS_TO_TIME(1000));
    assert_int_equal(FBP_TIME_SECOND, FBP_MICROSECONDS_TO_TIME(1000000));
    assert_int_equal(FBP_TIME_SECOND, FBP_NANOSECONDS_TO_TIME(1000000000));
}

static void test_abs(void **state) {
    (void) state;
    assert_int_equal(FBP_TIME_SECOND, FBP_TIME_ABS(FBP_TIME_SECOND));
    assert_int_equal(FBP_TIME_SECOND, FBP_TIME_ABS(-FBP_TIME_SECOND));
}

static void test_round_nearest(void **state) {
    (void) state;
    assert_int_equal(1, FBP_TIME_TO_COUNTER(FBP_TIME_SECOND, 1));
    assert_int_equal(1, FBP_TIME_TO_COUNTER(FBP_TIME_SECOND + 1, 1));
    assert_int_equal(1, FBP_TIME_TO_COUNTER(FBP_TIME_SECOND - 1, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER(-FBP_TIME_SECOND, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER(-FBP_TIME_SECOND + 1, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER(-FBP_TIME_SECOND - 1, 1));
}

static void test_round_zero(void **state) {
    (void) state;
    assert_int_equal(1, FBP_TIME_TO_COUNTER_RZERO(FBP_TIME_SECOND, 1));
    assert_int_equal(1, FBP_TIME_TO_COUNTER_RZERO(FBP_TIME_SECOND + 1, 1));
    assert_int_equal(0, FBP_TIME_TO_COUNTER_RZERO(FBP_TIME_SECOND - 1, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER_RZERO(-FBP_TIME_SECOND, 1));
    assert_int_equal(0, FBP_TIME_TO_COUNTER_RZERO(-FBP_TIME_SECOND + 1, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER_RZERO(-FBP_TIME_SECOND - 1, 1));
}

static void test_round_inf(void **state) {
    (void) state;
    assert_int_equal(1, FBP_TIME_TO_COUNTER_RINF(FBP_TIME_SECOND, 1));
    assert_int_equal(2, FBP_TIME_TO_COUNTER_RINF(FBP_TIME_SECOND + 1, 1));
    assert_int_equal(1, FBP_TIME_TO_COUNTER_RINF(FBP_TIME_SECOND - 1, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER_RINF(-FBP_TIME_SECOND, 1));
    assert_int_equal(-1, FBP_TIME_TO_COUNTER_RINF(-FBP_TIME_SECOND + 1, 1));
    assert_int_equal(-2, FBP_TIME_TO_COUNTER_RINF(-FBP_TIME_SECOND - 1, 1));
}

static void test_counter(void **state) {
    (void) state;
    uint32_t frequency = fbp_time_counter_frequency();
    assert_true(frequency >= 1000LL);  // recommendation
    // assert_true(frequency < 10000000000LL);  // requirement, but limited by 32-bit to 4 GHz
}

static void test_utc(void **state) {
    (void) state;
    int64_t t = fbp_time_utc();
    // Set to check for 2021, update on 2022 Jan 1.
    int64_t year_offset = 2021 - 2018;
    assert_true(t > (FBP_TIME_YEAR * year_offset));
    assert_true(t < (FBP_TIME_YEAR * (year_offset + 1)));
}

static void test_str(void **state) {
    (void) state;
    char s[30];
    assert_int_equal(26, fbp_time_to_str(0, s, sizeof(s)));
    assert_string_equal("2018-01-01T00:00:00.000000", s);
    assert_int_equal(19, fbp_time_to_str(0, s, 20));
    assert_string_equal("2018-01-01T00:00:00", s);

    fbp_time_to_str(FBP_TIME_SECOND, s, sizeof(s));
    assert_string_equal("2018-01-01T00:00:01.000000", s);
    fbp_time_to_str(FBP_TIME_SECOND * 60 * 60 * 24, s, sizeof(s));
    assert_string_equal("2018-01-02T00:00:00.000000", s);

    fbp_time_to_str(117133546395387584LL, s, sizeof(s));
    assert_string_equal("2021-06-16T14:31:56.002794", s);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_constants),
            cmocka_unit_test(test_f64),
            cmocka_unit_test(test_f32),
            cmocka_unit_test(test_convert_time_to),
            cmocka_unit_test(test_convert_to_time),
            cmocka_unit_test(test_abs),
            cmocka_unit_test(test_round_nearest),
            cmocka_unit_test(test_round_zero),
            cmocka_unit_test(test_round_inf),
            cmocka_unit_test(test_counter),
            cmocka_unit_test(test_utc),
            cmocka_unit_test(test_str),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
