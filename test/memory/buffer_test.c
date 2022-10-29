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
#include "fitterbap/memory/buffer.h"
#include "fitterbap/cdef.h"

fbp_size_t SIZES1[] = {8, 7, 6, 5, 4, 3, 2, 1};


static void init_alloc_free_one(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 10);
    assert_non_null(b);
    assert_int_equal(32, fbp_buffer_capacity(b));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void init_alloc_until_empty(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b[8];
    for (int i = 0; i < 8; ++i) {
        b[i] = fbp_buffer_alloc(a, 30);
    }
    assert_ptr_not_equal(b[0], b[1]);
    expect_assert_failure(fbp_buffer_alloc(a, 30));
    for (int i = 0; i < 8; ++i) {
        fbp_buffer_free(b[i]);
        assert_ptr_equal(b[i], fbp_buffer_alloc(a, 30));
    }
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void init_alloc_unsafe_until_empty(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b[8];
    for (int i = 0; i < 8; ++i) {
        b[i] = fbp_buffer_alloc_unsafe(a, 30);
    }
    assert_ptr_not_equal(b[0], b[1]);
    assert_ptr_equal(0, fbp_buffer_alloc_unsafe(a, 30));
    for (int i = 0; i < 8; ++i) {
        fbp_buffer_free(b[i]);
        assert_ptr_equal(b[i], fbp_buffer_alloc_unsafe(a, 30));
    }
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void init_alloc_free_around(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b;
    for (int i = 0; i < 16; ++i) {
        b = fbp_buffer_alloc(a, 30);
        fbp_buffer_free(b);
    }
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}


static void buffer_write_str(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    assert_int_equal(32, fbp_buffer_capacity(b));
    assert_int_equal(0, fbp_buffer_length(b));
    assert_int_equal(32, fbp_buffer_write_remaining(b));
    assert_int_equal(0, fbp_buffer_read_remaining(b));
    assert_int_equal(0, fbp_buffer_cursor_get(b));
    fbp_buffer_write_str_truncate(b, "hello");
    assert_int_equal(5, fbp_buffer_length(b));
    assert_int_equal(27, fbp_buffer_write_remaining(b));
    assert_int_equal(0, fbp_buffer_read_remaining(b));
    assert_int_equal(5, fbp_buffer_cursor_get(b));
    fbp_buffer_write_str(b, " world!");
    assert_memory_equal("hello world!", b->data, 12);
    assert_false(fbp_buffer_write_str_truncate(b, "this is a very long message which will be successfully truncated"));
    assert_int_equal(32, fbp_buffer_length(b));
    assert_int_equal(0, fbp_buffer_write_remaining(b));
    assert_int_equal(0, fbp_buffer_read_remaining(b));
    assert_int_equal(32, fbp_buffer_cursor_get(b));
    assert_false(fbp_buffer_write_str_truncate(b, "!"));
    expect_assert_failure(fbp_buffer_write_str(b, "!"));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_little_endian(void **state) {
    (void) state;
    uint8_t expect[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write_u8(b, (uint8_t) 0x01U);
    fbp_buffer_write_u16_le(b, (uint16_t) 0x0302U);
    fbp_buffer_write_u32_le(b, (uint32_t) 0x07060504U);
    fbp_buffer_write_u64_le(b, (uint64_t) 0x0f0e0d0c0b0a0908U);
    assert_memory_equal(expect, b->data, sizeof(expect));
    fbp_buffer_cursor_set(b, 0);
    assert_int_equal(0x01U, fbp_buffer_read_u8(b));
    assert_int_equal(0x0302U, fbp_buffer_read_u16_le(b));
    assert_int_equal(0x07060504U, fbp_buffer_read_u32_le(b));
    assert_int_equal(0x0f0e0d0c0b0a0908U, fbp_buffer_read_u64_le(b));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_big_endian(void **state) {
    (void) state;
    uint8_t expect[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write_u8(b, (uint8_t) 0x01U);
    fbp_buffer_write_u16_be(b, (uint16_t) 0x0203U);
    fbp_buffer_write_u32_be(b, (uint32_t) 0x04050607U);
    fbp_buffer_write_u64_be(b, (uint64_t) 0x08090a0b0c0d0e0fU);
    assert_memory_equal(expect, b->data, sizeof(expect));
    fbp_buffer_cursor_set(b, 0);
    assert_int_equal(0x01U, fbp_buffer_read_u8(b));
    assert_int_equal(0x0203U, fbp_buffer_read_u16_be(b));
    assert_int_equal(0x04050607U, fbp_buffer_read_u32_be(b));
    assert_int_equal(0x08090a0b0c0d0e0fU, fbp_buffer_read_u64_be(b));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_write_read(void **state) {
    (void) state;
    uint8_t const wr[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    uint8_t rd[sizeof(wr)];
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write(b, wr, sizeof(wr));
    fbp_buffer_cursor_set(b, 0);
    assert_int_equal(sizeof(wr), fbp_buffer_length(b));
    assert_memory_equal(wr, b->data, sizeof(wr));
    fbp_buffer_read(b, rd, sizeof(wr));
    assert_memory_equal(wr, rd, sizeof(wr));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_write_past_end(void **state) {
    (void) state;
    uint8_t const wr[32] = {0};
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write(b, wr, sizeof(wr));
    expect_assert_failure(fbp_buffer_write(b, wr, 1));
    expect_assert_failure(fbp_buffer_write_u8(b, 1));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_read_past_end(void **state) {
    (void) state;
    uint8_t rd[32];
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write_str_truncate(b, "hello");
    expect_assert_failure(fbp_buffer_read(b, rd, 1));
    expect_assert_failure(fbp_buffer_read_u8(b));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_overwrite(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    fbp_buffer_write_str_truncate(b, "hello great world!");
    assert_int_equal(18, fbp_buffer_length(b));
    assert_int_equal(14, fbp_buffer_write_remaining(b));
    fbp_buffer_cursor_set(b, 6);
    assert_int_equal(26, fbp_buffer_write_remaining(b));
    fbp_buffer_write_str_truncate(b, "weird");
    assert_int_equal(18, fbp_buffer_length(b));
    assert_int_equal(21, fbp_buffer_write_remaining(b));
    assert_int_equal(7, fbp_buffer_read_remaining(b));
    assert_int_equal(11, fbp_buffer_cursor_get(b));
    assert_memory_equal("hello weird world!", b->data, 18);
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_reserve(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b = fbp_buffer_alloc(a, 30);
    b->reserve = 27; // leaves 5 total_bytes
    fbp_buffer_write_str_truncate(b, "hello world!");
    assert_int_equal(5, fbp_buffer_length(b));
    expect_assert_failure(fbp_buffer_write_u8(b, 1));
    b->reserve = 0;
    fbp_buffer_write_u8(b, 1);
    assert_int_equal(6, fbp_buffer_length(b));
    fbp_buffer_free(b);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

static void buffer_copy(void **state) {
    (void) state;
    struct fbp_buffer_allocator_s * a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    struct fbp_buffer_s * b1 = fbp_buffer_alloc(a, 30);
    struct fbp_buffer_s * b2 = fbp_buffer_alloc(a, 30);
    fbp_buffer_write_str(b1, "hello world!");
    fbp_buffer_cursor_set(b1, 6);
    fbp_buffer_copy(b2, b1, fbp_buffer_read_remaining(b1));
    assert_int_equal(6, fbp_buffer_length(b2));
    assert_memory_equal("world!", b2->data, 6);
    fbp_buffer_free(b1);
    fbp_buffer_free(b2);
    fbp_buffer_allocator_finalize(a);
    fbp_free(a);
}

struct erase_s {
    struct fbp_buffer_allocator_s * a;
    struct fbp_buffer_s * b;
};

static int setup_erase(void ** state) {
    struct erase_s * self = (struct erase_s *) test_calloc(1, sizeof(struct erase_s));
    self->a = fbp_buffer_allocator_new(SIZES1, FBP_ARRAY_SIZE(SIZES1));
    self->b = fbp_buffer_alloc(self->a, 30);
    *state = self;
    return 0;
}

static int teardown_erase(void ** state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_free(self->b);
    fbp_buffer_allocator_finalize(self->a);
    fbp_free(self->a);
    test_free(self);
    return 0;
}

static void buffer_erase_cursor_at_end(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    fbp_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, fbp_buffer_length(self->b));
    assert_int_equal(20, fbp_buffer_write_remaining(self->b));
    assert_int_equal(0, fbp_buffer_read_remaining(self->b));
    assert_int_equal(12, fbp_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_cursor_in_middle(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    fbp_buffer_cursor_set(self->b, 8);
    fbp_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, fbp_buffer_length(self->b));
    assert_int_equal(26, fbp_buffer_write_remaining(self->b));
    assert_int_equal(6, fbp_buffer_read_remaining(self->b));
    assert_int_equal(6, fbp_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_cursor_before(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    fbp_buffer_cursor_set(self->b, 1);
    fbp_buffer_erase(self->b, 6, 11);
    assert_int_equal(12, fbp_buffer_length(self->b));
    assert_int_equal(31, fbp_buffer_write_remaining(self->b));
    assert_int_equal(11, fbp_buffer_read_remaining(self->b));
    assert_int_equal(1, fbp_buffer_cursor_get(self->b));
    assert_memory_equal("hello world!", self->b->data, 12);
}

static void buffer_erase_invalid(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    expect_assert_failure(fbp_buffer_erase(self->b, 6, 30));
    fbp_buffer_erase(self->b, 6, 6);
    assert_int_equal(17, fbp_buffer_length(self->b));
    expect_assert_failure(fbp_buffer_erase(self->b, 6, -1));
    expect_assert_failure(fbp_buffer_erase(self->b, -1, 6));
}

static void buffer_erase_all(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    fbp_buffer_erase(self->b, 0, fbp_buffer_length(self->b));
    assert_int_equal(0, fbp_buffer_length(self->b));
    assert_int_equal(0, fbp_buffer_cursor_get(self->b));
}

static void buffer_erase_to_length(void **state) {
    struct erase_s *self = (struct erase_s *) *state;
    fbp_buffer_write_str(self->b, "hello good world!");
    fbp_buffer_erase(self->b, 5, self->b->length);
    assert_int_equal(5, fbp_buffer_length(self->b));
    assert_int_equal(5, fbp_buffer_cursor_get(self->b));
}

static void basic_test(struct fbp_buffer_s * b) {
    assert_int_equal(32, fbp_buffer_capacity(b));
    assert_int_equal(0, fbp_buffer_length(b));
    assert_int_equal(0, fbp_buffer_cursor_get(b));
    fbp_buffer_write_str_truncate(b, "hello world!");
    assert_memory_equal("hello world!", b->data, 12);
}

static void buffer_static_declare(void **state) {
    (void) state;
    FBP_BUFFER_STATIC_DECLARE(b, 32);
    FBP_BUFFER_STATIC_INITIALIZE(b);
    basic_test(&b);
}

static void buffer_static_define(void **state) {
    (void) state;
    FBP_BUFFER_STATIC_DEFINE(b, 32);
    basic_test(&b);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(init_alloc_free_one),
            cmocka_unit_test(init_alloc_until_empty),
            cmocka_unit_test(init_alloc_unsafe_until_empty),
            cmocka_unit_test(init_alloc_free_around),
            cmocka_unit_test(buffer_write_str),
            cmocka_unit_test(buffer_little_endian),
            cmocka_unit_test(buffer_big_endian),
            cmocka_unit_test(buffer_write_read),
            cmocka_unit_test(buffer_write_past_end),
            cmocka_unit_test(buffer_read_past_end),
            cmocka_unit_test(buffer_overwrite),
            cmocka_unit_test(buffer_reserve),
            cmocka_unit_test(buffer_copy),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_at_end, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_in_middle, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_cursor_before, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_invalid, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_all, setup_erase, teardown_erase),
            cmocka_unit_test_setup_teardown(buffer_erase_to_length, setup_erase, teardown_erase),
            cmocka_unit_test(buffer_static_declare),
            cmocka_unit_test(buffer_static_define),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
