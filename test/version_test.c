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
#include <string.h>
#include "fitterbap/version.h"

static void test_encode_u32(void ** state) {
    (void) state;
    assert_int_equal(0x01000000, FBP_VERSION_ENCODE_U32(1, 0, 0));
    assert_int_equal(0x00010000, FBP_VERSION_ENCODE_U32(0, 1, 0));
    assert_int_equal(0x00000001, FBP_VERSION_ENCODE_U32(0, 0, 1));
    assert_int_equal(0xff000000, FBP_VERSION_ENCODE_U32(0xff, 0, 0));
    assert_int_equal(0x00ff0000, FBP_VERSION_ENCODE_U32(0, 0xff, 0));
    assert_int_equal(0x0000ffff, FBP_VERSION_ENCODE_U32(0, 0, 0xffff));

    assert_int_equal(0x00ff0000, FBP_VERSION_ENCODE_U32(0, 0xffffffff, 0));
    assert_int_equal(0x0000ffff, FBP_VERSION_ENCODE_U32(0, 0, 0xffffffff));
}

static void test_decode_u32(void ** state) {
    (void) state;
    assert_int_equal(1, FBP_VERSION_DECODE_U32_MAJOR(0x01000000));
    assert_int_equal(1, FBP_VERSION_DECODE_U32_MINOR(0x00010000));
    assert_int_equal(1, FBP_VERSION_DECODE_U32_PATCH(0x00000001));
}

static void test_encode_str(void ** state) {
    (void) state;
    assert_string_equal("1.2.3", FBP_VERSION_ENCODE_STR(1, 2, 3));
    assert_string_equal("255.255.65535", FBP_VERSION_ENCODE_STR(255, 255, 65535));
}

static void test_convert_u32_to_str(void ** state) {
    (void) state;
    char s[FBP_VERSION_STR_LENGTH_MAX];
    fbp_version_u32_to_str(0x01020003, s, sizeof(s));
    assert_string_equal("1.2.3", s);
    fbp_version_u32_to_str(0xffffffff, s, sizeof(s));
    assert_string_equal("255.255.65535", s);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_encode_u32),
            cmocka_unit_test(test_decode_u32),
            cmocka_unit_test(test_encode_str),
            cmocka_unit_test(test_convert_u32_to_str),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
