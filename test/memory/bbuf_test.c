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
#include "fitterbap/memory/bbuf.h"
#include "fitterbap.h"


static void u8_unsafe(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u8(&p1, 42);
    assert_ptr_equal(b + 1, p1);
    assert_int_equal(42, *b);
    assert_int_equal(42, fbp_bbuf_unsafe_decode_u8(&p2));
    assert_ptr_equal(p1, p2);
}

static void u16_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u16_be(&p1, 0x1122);
    assert_ptr_equal(b + 2, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x1122, fbp_bbuf_unsafe_decode_u16_be(&p2));
    assert_ptr_equal(p1, p2);
}

static void u16_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u16_le(&p1, 0x1122);
    assert_ptr_equal(b + 2, p1);
    assert_int_equal(0x22, b[0]);
    assert_int_equal(0x11, b[1]);
    assert_int_equal(0x1122, fbp_bbuf_unsafe_decode_u16_le(&p2));
    assert_ptr_equal(p1, p2);
}

static void u32_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u32_be(&p1, 0x11223344);
    assert_ptr_equal(b + 4, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x33, b[2]);
    assert_int_equal(0x44, b[3]);
    assert_int_equal(0x11223344, fbp_bbuf_unsafe_decode_u32_be(&p2));
    assert_ptr_equal(p1, p2);
}

static void u32_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u32_le(&p1, 0x11223344);
    assert_ptr_equal(b + 4, p1);
    assert_int_equal(0x44, b[0]);
    assert_int_equal(0x33, b[1]);
    assert_int_equal(0x22, b[2]);
    assert_int_equal(0x11, b[3]);
    assert_int_equal(0x11223344, fbp_bbuf_unsafe_decode_u32_le(&p2));
    assert_ptr_equal(p1, p2);
}

static void u64_unsafe_be(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u64_be(&p1, 0x1122334455667788llu);
    assert_ptr_equal(b + 8, p1);
    assert_int_equal(0x11, b[0]);
    assert_int_equal(0x22, b[1]);
    assert_int_equal(0x33, b[2]);
    assert_int_equal(0x44, b[3]);
    assert_int_equal(0x55, b[4]);
    assert_int_equal(0x66, b[5]);
    assert_int_equal(0x77, b[6]);
    assert_int_equal(0x88, b[7]);
    assert_int_equal(0x1122334455667788llu, fbp_bbuf_unsafe_decode_u64_be(&p2));
    assert_ptr_equal(p1, p2);
}

static void u64_unsafe_le(void **state) {
    (void) state;
    uint8_t b[16];
    uint8_t * p1 = b;
    uint8_t const * p2 = b;
    fbp_bbuf_unsafe_encode_u64_le(&p1, 0x1122334455667788llu);
    assert_ptr_equal(b + 8, p1);
    assert_int_equal(0x88, b[0]);
    assert_int_equal(0x77, b[1]);
    assert_int_equal(0x66, b[2]);
    assert_int_equal(0x55, b[3]);
    assert_int_equal(0x44, b[4]);
    assert_int_equal(0x33, b[5]);
    assert_int_equal(0x22, b[6]);
    assert_int_equal(0x11, b[7]);
    assert_int_equal(0x1122334455667788llu, fbp_bbuf_unsafe_decode_u64_le(&p2));
    assert_ptr_equal(p1, p2);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(u8_unsafe),
            cmocka_unit_test(u16_unsafe_be),
            cmocka_unit_test(u16_unsafe_le),
            cmocka_unit_test(u32_unsafe_be),
            cmocka_unit_test(u32_unsafe_le),
            cmocka_unit_test(u64_unsafe_be),
            cmocka_unit_test(u64_unsafe_le),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
