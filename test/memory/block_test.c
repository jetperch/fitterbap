/*
 * Copyright 2017-2021 Jetperch LLC
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
#include "fitterbap/memory/block.h"
#include "fitterbap/cdef.h"

struct test_s {
    struct fbp_mblock_s * s;
    void * memory;
};

static int setup1(void ** state) {
    struct test_s * t = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    assert_non_null(t);
    t->memory = test_calloc(1, 256);
    assert_non_null(t->memory);
    t->s = test_calloc(1, fbp_mblock_instance_size(256, 8));
    assert_non_null(t->s);
    assert_int_equal(0, fbp_mblock_initialize(t->s, t->memory, 256, 8));
    *state = t;
    return 0;
}

static int teardown1(void ** state) {
    struct test_s * t = (struct test_s *) *state;
    fbp_mblock_finalize(t->s);
    test_free(t->s);
    test_free(t->memory);
    test_free(t);
    return 0;
}

static void alloc_free_alloc_free(void **state) {
    struct test_s * t = (struct test_s *) *state;
    void * p1 = fbp_mblock_alloc(t->s, 1);
    fbp_mblock_free(t->s, p1, 1);
    void * p2 = fbp_mblock_alloc(t->s, 1);
    fbp_mblock_free(t->s, p2, 1);
    assert_ptr_equal(p1, p2);
}

static void alloc_until_full(void **state) {
    struct test_s * t = (struct test_s *) *state;
    uint8_t * p1 = (uint8_t *) fbp_mblock_alloc(t->s, 1);
    uint8_t * p2 = (uint8_t *) fbp_mblock_alloc(t->s, 1);
    assert_ptr_equal(p1 + 8, p2);
    (uint8_t *) fbp_mblock_alloc(t->s, 8 * 30); // fill
    expect_assert_failure(fbp_mblock_alloc(t->s, 1));
}

static void alloc_until_full_unsafe(void **state) {
    struct test_s * t = (struct test_s *) *state;
    uint8_t * p1 = (uint8_t *) fbp_mblock_alloc(t->s, 128);
    assert_non_null(p1);
    uint8_t * p2 = (uint8_t *) fbp_mblock_alloc(t->s, 64);
    assert_non_null(p2);
    assert_null(fbp_mblock_alloc_unsafe(t->s, 128));
    uint8_t * p3 = (uint8_t *) fbp_mblock_alloc_unsafe(t->s, 64);
    assert_non_null(p3);
}

static void alloc_free_middle_alloc_smaller(void **state) {
    struct test_s * t = (struct test_s *) *state;
    uint8_t * p[256/32];
    for (int i = 0; i < (int) FBP_ARRAY_SIZE(p); ++i) {
        p[i] = fbp_mblock_alloc(t->s, 32);
    }
    fbp_mblock_free(t->s, p[1], 32);
    fbp_mblock_free(t->s, p[3], 32);
    fbp_mblock_alloc(t->s, 16);
    fbp_mblock_alloc(t->s, 16);
    fbp_mblock_alloc(t->s, 16);
    fbp_mblock_alloc(t->s, 16);
}


static void alloc_fragmentation(void **state) {
    struct test_s * t = (struct test_s *) *state;
    uint8_t * p[256/32];
    for (int i = 0; i < (int) FBP_ARRAY_SIZE(p); ++i) {
        p[i] = fbp_mblock_alloc(t->s, 32);
    }
    fbp_mblock_free(t->s, p[1], 32);
    fbp_mblock_free(t->s, p[3], 32);
    expect_assert_failure(fbp_mblock_alloc(t->s, 64));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(alloc_free_alloc_free, setup1, teardown1),
            cmocka_unit_test_setup_teardown(alloc_until_full, setup1, teardown1),
            cmocka_unit_test_setup_teardown(alloc_until_full_unsafe, setup1, teardown1),
            cmocka_unit_test_setup_teardown(alloc_free_middle_alloc_smaller, setup1, teardown1),
            cmocka_unit_test_setup_teardown(alloc_fragmentation, setup1, teardown1),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
