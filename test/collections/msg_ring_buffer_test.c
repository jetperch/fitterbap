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

#include "../hal_test_impl.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include "fitterbap/collections/ring_buffer_msg.h"

#include <stdio.h>


#define SZ (36)


struct test_s {
    struct fbp_rbm_s mrb;
    uint8_t b[1024];
};

static int setup(void ** state) {
    struct test_s *self = NULL;
    self = (struct test_s *) test_calloc(1, sizeof(struct test_s));
    assert_true(SZ <= sizeof(self->b));
    fbp_rbm_init(&self->mrb, self->b, SZ);
    *state = self;
    return 0;
}

static int teardown(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    test_free(self);
    return 0;
}

static void test_initial_state(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_int_equal(0, self->mrb.head);
    assert_int_equal(0, self->mrb.tail);
    assert_int_equal(0, self->mrb.count);
    assert_int_equal(SZ, self->mrb.buf_size);
    assert_ptr_equal(self->b, self->mrb.buf);
}

static void test_alloc_until_full(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    assert_ptr_equal(self->b + 4, fbp_rbm_alloc(&self->mrb, 8));
    assert_ptr_equal(self->b + 16, fbp_rbm_alloc(&self->mrb, 8));
    assert_ptr_equal(NULL, fbp_rbm_alloc(&self->mrb, 8));
    assert_ptr_equal(self->b + 28, fbp_rbm_alloc(&self->mrb, 2));
    assert_ptr_equal(NULL, fbp_rbm_alloc(&self->mrb, 1));

    uint32_t sz = 0;
    assert_ptr_equal(self->b + 4, fbp_rbm_pop(&self->mrb, &sz));
    assert_int_equal(8, sz);
    assert_ptr_equal(self->b + 16, fbp_rbm_pop(&self->mrb, &sz));
    assert_int_equal(8, sz);
    assert_ptr_equal(self->b + 28, fbp_rbm_pop(&self->mrb, &sz));
    assert_int_equal(2, sz);
}

static void test_alloc_sizes(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint8_t * b;
    uint32_t sz_out = 0;

    for (uint32_t sz = 1; sz < ((SZ / 2) - 6); ++sz) {
        for (int i = 0; i < 32; ++i) {
            b = fbp_rbm_alloc(&self->mrb, sz);
            assert_non_null(b);
            b[0] = i;
            b = fbp_rbm_pop(&self->mrb, &sz_out);
            assert_non_null(b);
            assert_int_equal(sz, sz_out);
        }
    }
}

static void test_alloc_sizes_keep_not_empty(void ** state) {
    // brute force edge case test
    struct test_s *self = (struct test_s *) *state;
    uint8_t * b;
    uint32_t idx_max = 4000;
    uint32_t outstanding = 12;
    uint32_t sz = 0;

    fbp_rbm_init(&self->mrb, self->b, sizeof(self->b));

    for (uint32_t idx = 0; idx < idx_max; ++idx) {
        if (idx < (idx_max - outstanding)) {
            sz = idx / 64 + 1;
            b = fbp_rbm_alloc(&self->mrb, sz);
            assert_non_null(b);
            b[0] = (uint8_t) (idx & 0xff);
        }
        if (idx >= outstanding) {
            b = fbp_rbm_pop(&self->mrb, &sz);
            assert_non_null(b);
            assert_int_equal(sz, ((idx - outstanding) / 64) + 1);
            assert_int_equal(b[0], (idx - outstanding) & 0xff);
        }
    }
}

static void test_clear(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint32_t sz = 1;
    assert_non_null(fbp_rbm_alloc(&self->mrb, 8));
    fbp_rbm_clear(&self->mrb);
    assert_null(fbp_rbm_pop(&self->mrb, &sz));
    assert_int_equal(0, sz);
}

static void test_alloc_halves(void ** state) {
    struct test_s *self = (struct test_s *) *state;
    uint32_t sz = 1;
    assert_non_null(fbp_rbm_alloc(&self->mrb, SZ/2));
    assert_non_null(fbp_rbm_pop(&self->mrb, &sz));
    assert_non_null(fbp_rbm_alloc(&self->mrb, SZ/2));
    assert_non_null(fbp_rbm_pop(&self->mrb, &sz));
    assert_non_null(fbp_rbm_alloc(&self->mrb, SZ/2));
    assert_non_null(fbp_rbm_pop(&self->mrb, &sz));
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_initial_state, setup, teardown),
            cmocka_unit_test_setup_teardown(test_alloc_until_full, setup, teardown),
            cmocka_unit_test_setup_teardown(test_alloc_sizes, setup, teardown),
            cmocka_unit_test_setup_teardown(test_alloc_sizes_keep_not_empty, setup, teardown),
            cmocka_unit_test_setup_teardown(test_clear, setup, teardown),
            cmocka_unit_test_setup_teardown(test_alloc_halves, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
