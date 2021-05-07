/*
 * Copyright 2021 Jetperch LLC
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
#include "fitterbap/platform.h"
#include "fitterbap/comm/wave_sink_port.h"

#include "port_hal.inc"

static void test_factory(void ** state) {
    (void) state;
    struct fbp_port_api_s * p = fbp_wave_sink_factory();
    assert_non_null(p);
    assert_non_null(p->meta);
    assert_non_null(p->initialize);
    assert_non_null(p->finalize);
    assert_non_null(p->on_event);
    assert_non_null(p->on_recv);
    p->finalize(p);
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_factory),
            //cmocka_unit_test(),
            //cmocka_unit_test(),
            //cmocka_unit_test(),
            //cmocka_unit_test(),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
