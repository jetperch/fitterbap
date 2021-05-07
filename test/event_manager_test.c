/*
 * Copyright 2018-2021 Jetperch LLC
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
#include <stdlib.h>
#include "fitterbap/event_manager.h"
#include "fitterbap/time.h"
#include "hal_test_impl.h"


#define SETUP() \
    (void) state;   \
    struct fbp_evm_s * evm = fbp_evm_allocate()

#define TEARDOWN() \
    fbp_evm_free(evm)

void cbk_full(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

void cbk1(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

void cbk2(void * user_data, int32_t event_id) {
    (void) user_data;
    check_expected(event_id);
}

static void test_allocate(void **state) {
    SETUP();
    assert_int_equal(-1, fbp_evm_interval_next(evm, 10));
    assert_int_equal(FBP_TIME_MIN, fbp_evm_time_next(evm));
    fbp_evm_process(evm, 10);
    TEARDOWN();
}

static void test_single_event(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk_full, NULL));
    assert_int_equal(10, fbp_evm_time_next(evm));
    assert_int_equal(8, fbp_evm_interval_next(evm, 2));

    fbp_evm_process(evm, 9);
    expect_value(cbk_full, event_id, 1);
    fbp_evm_process(evm, 10);
    TEARDOWN();
}

static void test_insert_two_events_in_order(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk1, NULL));
    assert_int_equal(2, fbp_evm_schedule(evm, 20, cbk2, NULL));
    assert_int_equal(10, fbp_evm_interval_next(evm, 0));

    expect_value(cbk1, event_id, 1);
    fbp_evm_process(evm, 10);
    expect_value(cbk2, event_id, 2);
    fbp_evm_process(evm, 20);
    assert_int_equal(-1, fbp_evm_interval_next(evm, 10));
    TEARDOWN();
}

static void test_insert_two_events_out_of_order(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 20, cbk2, NULL));
    assert_int_equal(20, fbp_evm_interval_next(evm, 0));
    assert_int_equal(2, fbp_evm_schedule(evm, 10, cbk1, NULL));
    assert_int_equal(10, fbp_evm_interval_next(evm, 0));

    expect_value(cbk1, event_id, 2);
    fbp_evm_process(evm, 10);
    expect_value(cbk2, event_id, 1);
    assert_int_equal(20, fbp_evm_time_next(evm));
    assert_int_equal(8, fbp_evm_interval_next(evm, 12));
    fbp_evm_process(evm, 20);
    assert_int_equal(-1, fbp_evm_interval_next(evm, 10));
    TEARDOWN();
}

static void test_insert_two_events_and_cancel_first(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk1, NULL));
    assert_int_equal(2, fbp_evm_schedule(evm, 20, cbk2, NULL));
    assert_int_equal(10, fbp_evm_interval_next(evm, 0));
    fbp_evm_cancel(evm, 1);
    expect_value(cbk2, event_id, 2);
    fbp_evm_process(evm, 20);
    assert_int_equal(-1, fbp_evm_interval_next(evm, 10));
    TEARDOWN();
}

int main(void) {
    hal_test_initialize();
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_allocate),
            cmocka_unit_test(test_single_event),
            cmocka_unit_test(test_insert_two_events_in_order),
            cmocka_unit_test(test_insert_two_events_out_of_order),
            cmocka_unit_test(test_insert_two_events_and_cancel_first),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
