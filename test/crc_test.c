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
#include "fitterbap/crc.h"


uint8_t const MSG0[] = { 0x00, 0x00, 0x00, 0x00 };
uint8_t const MSG_00[] = {0x00};
uint8_t const MSG_01[] = {0x01};
uint8_t const MSG_FF[] = {0xff};

// http://blog.xivo.fr/public/smbus_pec_crc8_test_vectors.txt
uint8_t const MSG2[] = { 0x61, 0x62, 0x63 };
// http://blog.xivo.fr/public/smbus_pec_crc8_test_vectors.txt
uint8_t const MSG3[] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                         0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                         0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                         0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };
// http://www.lammertbies.nl/comm/info/crc-calculation.html
uint8_t const MSG4[] = {'1', '2','3', '4', '5', '6', '7', '8', '9'};


static void crc_ccitt_8_well_known(void **state) {
    (void) state;
    assert_int_equal(0x74, fbp_crc_ccitt_8(0, MSG0, sizeof(MSG0)));
    assert_int_equal(0x30, fbp_crc_ccitt_8(0, MSG_00, sizeof(MSG_00)));
    assert_int_equal(0xa1, fbp_crc_ccitt_8(0, MSG_01, sizeof(MSG_01)));
    assert_int_equal(0xff, fbp_crc_ccitt_8(0, MSG_FF, sizeof(MSG_FF)));
    assert_int_equal(0xdb, fbp_crc_ccitt_8(0, MSG2, sizeof(MSG2)));
    assert_int_equal(0xe4, fbp_crc_ccitt_8(0, MSG3, sizeof(MSG3)));
    assert_int_equal(0x2f, fbp_crc_ccitt_8(0, MSG4, sizeof(MSG4)));
}

static void crc_ccitt_8_incremental(void **state) {
    (void) state;
    uint8_t crc = 0;
    crc = fbp_crc_ccitt_8(crc, MSG4, 5);
    crc = fbp_crc_ccitt_8(crc, MSG4 + 5, sizeof(MSG4) - 5);
    assert_int_equal(0x2f, crc);
}

static void crc_ccitt_8_invalid_args(void **state) {
    (void) state;
    assert_int_equal(0x00000000, fbp_crc_ccitt_8(0, 0, 4));
    assert_int_equal(0x00000000, fbp_crc_ccitt_8(0, MSG_00, 0));
}

static void crc_ccitt_16_well_known(void **state) {
    (void) state;
    assert_int_equal(0x7b3f, fbp_crc_ccitt_16(0, MSG0, sizeof(MSG0)));
    assert_int_equal(0x1e0f, fbp_crc_ccitt_16(0, MSG_00, sizeof(MSG_00)));
    assert_int_equal(0x0e2e, fbp_crc_ccitt_16(0, MSG_01, sizeof(MSG_01)));
    assert_int_equal(0x00ff, fbp_crc_ccitt_16(0, MSG_FF, sizeof(MSG_FF)));
    assert_int_equal(0xaeb5, fbp_crc_ccitt_16(0, MSG2, sizeof(MSG2)));
    assert_int_equal(0x39f9, fbp_crc_ccitt_16(0, MSG3, sizeof(MSG3)));
    assert_int_equal((uint16_t) ~0x29B1, fbp_crc_ccitt_16(0, MSG4, sizeof(MSG4)));
}

static void crc_ccitt_16_incremental(void **state) {
    (void) state;
    uint16_t crc = 0;
    crc = fbp_crc_ccitt_16(crc, MSG4, 5);
    crc = fbp_crc_ccitt_16(crc, MSG4 + 5, sizeof(MSG4) - 5);
    assert_int_equal((uint16_t) ~0x29B1, crc);
}

static void crc_ccitt_16_invalid_args(void **state) {
    (void) state;
    assert_int_equal(0x00000000, fbp_crc_ccitt_16(0, 0, 4));
    assert_int_equal(0x00000000, fbp_crc_ccitt_16(0, MSG_00, 0));
}

static void crc32_well_known(void **state) {
    (void) state;
    assert_int_equal(0x2144df1c, fbp_crc32(0, MSG0, sizeof(MSG0)));
    assert_int_equal(0xd202ef8d, fbp_crc32(0, MSG_00, sizeof(MSG_00)));
    assert_int_equal(0xa505df1b, fbp_crc32(0, MSG_01, sizeof(MSG_01)));
    assert_int_equal(0xff000000, fbp_crc32(0, MSG_FF, sizeof(MSG_FF)));
    assert_int_equal(0x352441c2, fbp_crc32(0, MSG2, sizeof(MSG2)));
    assert_int_equal(0x08053b40, fbp_crc32(0, MSG3, sizeof(MSG3)));
    assert_int_equal(0xCBF43926, fbp_crc32(0, MSG4, sizeof(MSG4)));
}

static void crc32_incremental(void **state) {
    (void) state;
    uint32_t crc = 0;
    crc = fbp_crc32(crc, MSG4, 5);
    crc = fbp_crc32(crc, MSG4 + 5, sizeof(MSG4) - 5);
    assert_int_equal(0xCBF43926, crc);
}

static void crc32_invalid_args(void **state) {
    (void) state;
    assert_int_equal(0x00000000, fbp_crc32(0, 0, 4));
    assert_int_equal(0x00000000, fbp_crc32(0, MSG_00, 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(crc_ccitt_8_well_known),
            cmocka_unit_test(crc_ccitt_8_incremental),
            cmocka_unit_test(crc_ccitt_8_invalid_args),

            cmocka_unit_test(crc_ccitt_16_well_known),
            cmocka_unit_test(crc_ccitt_16_incremental),
            cmocka_unit_test(crc_ccitt_16_invalid_args),

            cmocka_unit_test(crc32_well_known),
            cmocka_unit_test(crc32_incremental),
            cmocka_unit_test(crc32_invalid_args),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
