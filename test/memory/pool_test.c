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
#include "fitterbap/memory/pool.h"
#include <string.h> // memset


static int setup1(void ** state) {
    struct fbp_pool_s * self = test_calloc(1, fbp_pool_instance_size(1, 16));
    assert_int_equal(0, fbp_pool_initialize(self, 1, 16));
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct fbp_pool_s * s = (struct fbp_pool_s *) *state;
    fbp_pool_finalize(s);
    test_free(s);
    return 0;
}

static int setup2(void ** state) {
    struct fbp_pool_s * self = test_calloc(1, fbp_pool_instance_size(2, 16));
    assert_int_equal(0, fbp_pool_initialize(self, 2, 16));
    *state = self;
    return 0;
}

static void create(void **state) {
    struct fbp_pool_s * self = (struct fbp_pool_s *) *state;
    uint8_t * d1 = (uint8_t *) fbp_pool_alloc(self);
    d1[0] = 'h';
    assert_non_null(d1);
    fbp_pool_free(self, d1);
    uint8_t * d2 = (uint8_t *) fbp_pool_alloc(self);
    assert_ptr_equal(d1, d2);
    fbp_pool_free(self, d2);
}

static void alloc_too_many(void **state) {
    struct fbp_pool_s * self = (struct fbp_pool_s *) *state;
    assert_int_equal(0, fbp_pool_is_empty(self));
    fbp_pool_alloc(self);
    assert_int_equal(1, fbp_pool_is_empty(self));
    expect_assert_failure(fbp_pool_alloc(self));
}

static void alloc_too_many_unsafe(void **state) {
    struct fbp_pool_s * self = (struct fbp_pool_s *) *state;
    assert_int_equal(0, fbp_pool_is_empty(self));
    fbp_pool_alloc(self);
    assert_int_equal(1, fbp_pool_is_empty(self));
    assert_null(fbp_pool_alloc_unsafe(self));
}

static void alloc_multiple(void ** state) {
    struct fbp_pool_s * self = (struct fbp_pool_s *) *state;
    uint8_t * d1 = (uint8_t *) fbp_pool_alloc(self);
    assert_non_null(d1);
    void * d2 = fbp_pool_alloc(self);
    assert_non_null(d2);
    assert_ptr_not_equal(d1, d2);
    expect_assert_failure(fbp_pool_alloc(self));
    fbp_pool_free(self, d1);
    uint8_t * d3 = (uint8_t *) fbp_pool_alloc(self);
    assert_ptr_equal(d1, d3);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(create, setup1, teardown),
            cmocka_unit_test_setup_teardown(alloc_too_many, setup1, teardown),
            cmocka_unit_test_setup_teardown(alloc_too_many_unsafe, setup1, teardown),
            cmocka_unit_test_setup_teardown(alloc_multiple, setup2, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
