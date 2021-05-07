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
#include "fitterbap/lfsr.h"


static const uint8_t LFSR16_U8[] = {0x22, 0x47, 0x37, 0xc4, 0x9d, 0xe3, 0x15, 0x88, 0x52, 0xef, 0x16, 0x3e, 0xa1, 0x5f, 0x40, 0x41};
static const uint16_t LFSR16_U16[] = {0x4722, 0xc437, 0xe39d, 0x8815, 0xef52, 0x3e16, 0x5fa1, 0x4140};
static const uint32_t LFSR16_U32[] = {0x4722c437, 0xe39d8815};


struct fbp_lfsr_s lfsr;


static int setup(void ** state) {
    (void) state;
    lfsr.value = 0;
    return 0;
}

/*
static void fbp_lfsr_display_u8(void **state) {
    (void) state;
    int i;
    uint8_t v;
    assert_int_equal(0, fbp_lfsr_initialize(&lfsr));
    printf("static const uint8_t LFSR16_U8[] = {");
    for (int i = 0; i < LFSR16_LENGTH * 2; ++i) {
        if (i) {
            printf(", ");
        }
        if (0 == (i & 0xf)) {
            printf("\n    ");
        }
        v = fbp_lfsr_next_u8(&lfsr);
        printf("0x%02x", v);
    }
    printf("\n};\n");
}
*/

/*
static void fbp_lfsr_display_u16(void **state) {
    (void) state;
    int i;
    uint16_t v;
    assert_int_equal(0, fbp_lfsr_initialize(&lfsr));
    printf("static const uint8_t LFSR16_U16[] = {");
    for (int i = 0; i < LFSR16_LENGTH; ++i) {
        if (i) {
            printf(", ");
        }
        if (0 == (i & 0x7)) {
            printf("\n    ");
        }
        v = fbp_lfsr_next_u16(&lfsr);
        printf("0x%04x", v);
    }
    printf("\n};\n");
}
*/

static void test_fbp_lfsr_invalid(void **state) {
    (void) state;
    fbp_lfsr_initialize(&lfsr);
    expect_assert_failure(fbp_lfsr_next_u16(0));
}

static void test_fbp_lfsr_next_u8(void **state) {
    (void) state;
    uint8_t value;
    fbp_lfsr_initialize(&lfsr);
    value = fbp_lfsr_next_u8(&lfsr);
    assert_int_equal(LFSR16_U8[0], value);
    value = fbp_lfsr_next_u8(&lfsr);
    assert_int_equal(LFSR16_U8[1], value);

    assert_int_equal(LFSR16_U16[0], lfsr.value);
}

static void test_fbp_lfsr_next_u16(void **state) {
    (void) state;
    uint16_t value;
    fbp_lfsr_initialize(&lfsr);
    value = fbp_lfsr_next_u16(&lfsr);
    assert_int_equal(LFSR16_U16[0], value);
    assert_int_equal(LFSR16_U16[0], lfsr.value);
    value = fbp_lfsr_next_u16(&lfsr);
    assert_int_equal(LFSR16_U16[1], value);
    assert_int_equal(LFSR16_U16[1], lfsr.value);
}

static void test_fbp_lfsr_next_u32(void **state) {
    (void) state;
    uint32_t value;
    fbp_lfsr_initialize(&lfsr);
    value = fbp_lfsr_next_u32(&lfsr);
    assert_int_equal(LFSR16_U32[0], value);
    value = fbp_lfsr_next_u32(&lfsr);
    assert_int_equal(LFSR16_U32[1], value);
}

static void test_fbp_lfsr_seed_u16(void **state) {
    (void) state;
    uint16_t value;
    fbp_lfsr_initialize(&lfsr);
    fbp_lfsr_seed_u16(&lfsr, 26625);
    value = fbp_lfsr_next_u16(&lfsr);
    assert_int_equal(5185, value);
}

static void test_fbp_lfsr_u16_wrap(void **state) {
    (void) state;
    int i;
    uint16_t v1;
    uint16_t v2;

    fbp_lfsr_initialize(&lfsr);
    v1 = fbp_lfsr_next_u16(&lfsr);
    for (i = 0; i < 65535; ++i) {
        v2 = fbp_lfsr_next_u16(&lfsr);
    }
    assert_int_equal(v1, v2);
}

static void test_fbp_lfsr_follow(void **state) {
    (void) state;
    int i;
    struct fbp_lfsr_s s1;
    struct fbp_lfsr_s s2;
    uint8_t v;

    fbp_lfsr_initialize(&s1);
    fbp_lfsr_initialize(&s2);
    for (i = 0; i < 65535 * 2; ++i) {
        v = fbp_lfsr_next_u8(&s1);
        assert_int_equal(0, fbp_lfsr_follow_u8(&s2, v));
    }
}

static void test_fbp_lfsr_follow_valid(void **state) {
    (void) state;
    int i;
    int v;
    fbp_lfsr_initialize(&lfsr);
    expect_assert_failure(fbp_lfsr_follow_u8(0, 1));
    for (i = 0; i < 8; ++i) {
        v = fbp_lfsr_follow_u8(&lfsr, LFSR16_U8[i]);
        assert_int_equal(0, v);
    }
    v = fbp_lfsr_follow_u8(&lfsr, LFSR16_U8[10]);
    assert_int_equal(-1, v);
    for (int i = 11; i < 16; ++i) {
        v = fbp_lfsr_follow_u8(&lfsr, LFSR16_U8[i]);
        assert_int_equal(0, v);
    }
}

static void test_fbp_lfsr_follow_valid_sync(void **state) {
    (void) state;
    int i;
    int v;
    fbp_lfsr_initialize(&lfsr);
    for (i = 11; i < 16; ++i) {
        v = fbp_lfsr_follow_u8(&lfsr, LFSR16_U8[i]);
        assert_int_equal(0, v);
    }
}


int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_fbp_lfsr_invalid, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_next_u8, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_next_u16, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_next_u32, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_seed_u16, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_u16_wrap, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_follow, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_follow_valid, setup),
            cmocka_unit_test_setup(test_fbp_lfsr_follow_valid_sync, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
