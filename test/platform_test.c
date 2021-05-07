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
#include "fitterbap/platform.h"
#include "fitterbap/cdef.h"


static void test_clz_extremes(void **state) {
    (void) state;
    assert_int_equal(32, fbp_clz(0));
    assert_int_equal(0, fbp_clz(0x80000000));
}

static void test_clz_individual_bits(void **state) {
    (void) state;
    for (uint32_t bit = 0; bit < 32; ++bit) {
        uint32_t x = 1 << bit;
        assert_int_equal(31 - bit, fbp_clz(x));
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_clz_extremes),
            cmocka_unit_test(test_clz_individual_bits),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
