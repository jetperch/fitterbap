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
#include "fitterbap/pattern_32a.h"
#include "fitterbap/cdef.h"


struct fbp_pattern_32a_tx_s tx1;
struct fbp_pattern_32a_tx_s tx2;
struct fbp_pattern_32a_rx_s rx1;

const uint32_t PATTERN[] = {
        0x00000000,
        0xFFFF0000,
        0x00000001,
        0xFFFE0001,
        0x00000002,
        0xFFFD0002,
        0x00000004,
        0xFFFC0003,
};


static int setup(void ** state) {
    (void) state;
    fbp_pattern_32a_tx_initialize(&tx1);
    fbp_pattern_32a_tx_initialize(&tx2);
    fbp_pattern_32a_rx_initialize(&rx1);
    return 0;
}

static void test_tx_next(void **state) {
    (void) state;
    for (fbp_size_t i = 0; i < FBP_ARRAY_SIZE(PATTERN); ++i) {
        assert_int_equal(PATTERN[i], fbp_pattern_32a_tx_next(&tx1));
    }
}

static void test_tx_next_validate_shift(void **state) {
    (void) state;
    uint32_t expected_value = 0;
    uint32_t value = 0;
    for (fbp_size_t i = 0; i < 50; ++i) {
        value = fbp_pattern_32a_tx_next(&tx1);
        assert_int_equal(expected_value, value);
        expected_value = (expected_value == 0) ? 1 : (expected_value << 1);
        fbp_pattern_32a_tx_next(&tx1);
    }
}

static void test_tx_buffer(void **state) {
    (void) state;
    uint32_t buffer[1024];
    fbp_pattern_32a_tx_buffer(&tx2, buffer, sizeof(buffer));
    for (fbp_size_t i = 0; i < FBP_ARRAY_SIZE(buffer); ++i) {
        assert_int_equal(buffer[i], fbp_pattern_32a_tx_next(&tx1));
    }
}

static void test_tx_buffer_multiple(void **state) {
    (void) state;
    uint32_t buffer[128];
    for (fbp_size_t i = 0; i < 128; ++i) {
        fbp_pattern_32a_tx_buffer(&tx2, buffer, i * 4);
        for (fbp_size_t k = 0; k < i; k++) {
            assert_int_equal(buffer[k], fbp_pattern_32a_tx_next(&tx1));
        }
    }
}

static void test_rx_next_start_from_shift(void **state) {
    (void) state;
    for (fbp_size_t i = 0; i < FBP_ARRAY_SIZE(PATTERN); ++i) {
        fbp_pattern_32a_rx_next(&rx1, PATTERN[i]);
    }
    assert_int_equal(FBP_ARRAY_SIZE(PATTERN), rx1.receive_count);
    assert_int_equal(0, rx1.resync_count);
    assert_int_equal(0, rx1.missing_count);
    assert_int_equal(0, rx1.duplicate_count);
    assert_int_equal(0, rx1.error_count);
}

static void test_rx_next_start_from_counter(void **state) {
    (void) state;
    for (fbp_size_t i = 1; i < FBP_ARRAY_SIZE(PATTERN); ++i) {
        fbp_pattern_32a_rx_next(&rx1, PATTERN[i]);
    }
    assert_int_equal(FBP_ARRAY_SIZE(PATTERN) - 1, rx1.receive_count);
    assert_int_equal(0, rx1.resync_count);
    assert_int_equal(0, rx1.missing_count);
    assert_int_equal(0, rx1.duplicate_count);
    assert_int_equal(0, rx1.error_count);
}

static void skip_case(int offset, int before, int skip, int after) {
    fbp_pattern_32a_tx_initialize(&tx1);
    fbp_pattern_32a_rx_initialize(&rx1);
    uint32_t value = 0;
    for (int i = 0; i < offset; ++i) {
        fbp_pattern_32a_tx_next(&tx1);
    }
    for (int i = 0; i < before; ++i) {
        value = fbp_pattern_32a_tx_next(&tx1);
        fbp_pattern_32a_rx_next(&rx1, value);
    }
    for (int i = 0; i < skip; ++i) {
        fbp_pattern_32a_tx_next(&tx1);
    }
    for (int i = 0; i < after; ++i) {
        value = fbp_pattern_32a_tx_next(&tx1);
        fbp_pattern_32a_rx_next(&rx1, value);
    }

    assert_int_equal(before + after, rx1.receive_count);
    assert_int_equal(1, rx1.resync_count);
    assert_int_equal(skip, rx1.missing_count);
    assert_int_equal(0, rx1.duplicate_count);
    assert_int_equal(1, rx1.error_count);
}

static void range_case(int offset1, int length1, int offset2, int length2) {
    fbp_pattern_32a_tx_initialize(&tx1);
    fbp_pattern_32a_rx_initialize(&rx1);
    uint32_t value = 0;
    for (int i = 0; i < offset1; ++i) {
        fbp_pattern_32a_tx_next(&tx1);
    }
    for (int i = 0; i < length1; ++i) {
        value = fbp_pattern_32a_tx_next(&tx1);
        fbp_pattern_32a_rx_next(&rx1, value);
    }
    fbp_pattern_32a_tx_initialize(&tx1);
    for (int i = 0; i < offset2; ++i) {
        fbp_pattern_32a_tx_next(&tx1);
    }
    for (int i = 0; i < length2; ++i) {
        value = fbp_pattern_32a_tx_next(&tx1);
        fbp_pattern_32a_rx_next(&rx1, value);
    }

    assert_int_equal(length1 + length2, rx1.receive_count);
    assert_int_equal(1, rx1.resync_count);
    if (offset1 + length1 > offset2) {
        assert_int_equal(offset1 + length1 - offset2, rx1.duplicate_count);
        assert_int_equal(0, rx1.missing_count);
    } else {
        assert_int_equal(0, rx1.duplicate_count);
        assert_int_equal(offset2 - (offset1 + length1), rx1.missing_count);
    }
    assert_int_equal(1, rx1.error_count);
}

static void test_rx_skip_cases(void **state) {
    (void) state;
    skip_case(0, 10, 1, 10);
    skip_case(0, 11, 1, 10);
    skip_case(0, 10, 2, 10);
    skip_case(0, 11, 2, 10);

    skip_case(0, 10, 6, 10);
    skip_case(0, 10, 7, 10);
    skip_case(0, 11, 6, 10);
    skip_case(0, 11, 7, 10);

    skip_case(7, 10, 6, 10);
    skip_case(7, 10, 7, 10);
    skip_case(7, 11, 6, 10);
    skip_case(7, 11, 7, 10);

    skip_case(8, 10, 6, 10);
    skip_case(8, 10, 7, 10);
    skip_case(8, 11, 6, 10);
    skip_case(8, 11, 7, 10);

    skip_case(0, 4, 60, 4);
    skip_case(0, 4, 61, 4);
    skip_case(1, 4, 61, 4);
    skip_case(1, 3, 61, 4);
    skip_case(1, 3, 62, 4);
    skip_case(0, 4, 1024, 4);
    skip_case(0, 4, (1 << 16), 4);
    skip_case(0, 3, (1 << 16), 4);
    skip_case(0, 4, (1 << 16) + 1, 4);
    skip_case(0, 3, (1 << 16) + 1, 4);
}

static void test_rx_duplicate_cases(void **state) {
    (void) state;
    range_case(0, 10, 5, 10); // duplicate 5
    range_case(0, 11, 6, 10); // duplicate 5
    range_case(0, 10, 4, 10); // duplicate 6
    range_case(0, 11, 5, 10); // duplicate 6

    range_case(0, 100, 4, 10);
    range_case(0, 100, 5, 10);
    range_case(0, 1026, 4, 10);
    range_case(0, 1027, 4, 10);
    range_case(0, 1026, 5, 10);
    range_case(0, 1027, 5, 10);
}

static void test_rx_buffer(void **state) {
    (void) state;
    uint32_t buffer[1024];
    fbp_pattern_32a_tx_buffer(&tx1, buffer, sizeof(buffer));
    fbp_pattern_32a_rx_buffer(&rx1, buffer, 100);
    fbp_pattern_32a_rx_buffer(&rx1, buffer + 50, 100);
    fbp_pattern_32a_rx_buffer(&rx1, buffer + 70, 100);
    assert_int_equal(75, rx1.receive_count);
    assert_int_equal(2, rx1.resync_count);
    assert_int_equal(25, rx1.missing_count);
    assert_int_equal(5, rx1.duplicate_count);
    assert_int_equal(2, rx1.error_count);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_tx_next, setup),
            cmocka_unit_test_setup(test_tx_next_validate_shift, setup),
            cmocka_unit_test_setup(test_tx_buffer, setup),
            cmocka_unit_test_setup(test_tx_buffer_multiple, setup),
            cmocka_unit_test_setup(test_rx_next_start_from_shift, setup),
            cmocka_unit_test_setup(test_rx_next_start_from_counter, setup),
            cmocka_unit_test_setup(test_rx_skip_cases, setup),
            cmocka_unit_test_setup(test_rx_duplicate_cases, setup),
            cmocka_unit_test_setup(test_rx_buffer, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
