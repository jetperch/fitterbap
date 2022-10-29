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
#include "fitterbap/platform.h"
#include "fitterbap/time.h"

int64_t count_ms_ = 0;

#define SETUP() \
    (void) state; \
    count_ms_ = 0;    \
    struct fbp_evm_s * evm = fbp_evm_allocate()

#define TEARDOWN() \
    fbp_evm_free(evm)


uint32_t fbp_time_counter_frequency_() {
    return 1000;
}

uint64_t fbp_time_counter_u64_() {
    return count_ms_;
}

uint32_t fbp_time_counter_u32_() {
    return (uint32_t) count_ms_;
}

void fbp_os_mutex_lock_(fbp_os_mutex_t mutex) {
    if (mutex) {
        check_expected_ptr(mutex);
    }
}

void fbp_os_mutex_unlock_(fbp_os_mutex_t mutex) {
    if (mutex) {
        check_expected_ptr(mutex);
    }
}

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
    assert_int_equal(INT64_MAX, fbp_evm_interval_next(evm, 10));
    assert_int_equal(FBP_TIME_MAX, fbp_evm_time_next(evm));
    assert_int_equal(0, fbp_evm_scheduled_event_count(evm));
    fbp_evm_process(evm, 10);
    TEARDOWN();
}

static void test_single_event(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk_full, NULL));
    assert_int_equal(1, fbp_evm_scheduled_event_count(evm));
    assert_int_equal(10, fbp_evm_time_next(evm));
    assert_int_equal(8, fbp_evm_interval_next(evm, 2));

    fbp_evm_process(evm, 9);
    expect_value(cbk_full, event_id, 1);
    fbp_evm_process(evm, 10);
    assert_int_equal(0, fbp_evm_scheduled_event_count(evm));
    TEARDOWN();
}

static void test_insert_two_events_in_order(void **state) {
    SETUP();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk1, NULL));
    assert_int_equal(2, fbp_evm_schedule(evm, 20, cbk2, NULL));
    assert_int_equal(10, fbp_evm_interval_next(evm, 0));

    expect_value(cbk1, event_id, 1);
    fbp_evm_process(evm, 10);
    assert_int_equal(20, fbp_evm_time_next(evm));
    assert_int_equal(10, fbp_evm_interval_next(evm, 10));

    expect_value(cbk2, event_id, 2);
    fbp_evm_process(evm, 20);
    assert_int_equal(INT64_MAX, fbp_evm_interval_next(evm, 10));
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
    assert_int_equal(INT64_MAX, fbp_evm_interval_next(evm, 10));
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
    assert_int_equal(INT64_MAX, fbp_evm_interval_next(evm, 10));
    TEARDOWN();
}

static void test_api(void **state) {
    SETUP();
    struct fbp_evm_api_s api;
    assert_int_equal(0, fbp_evm_api_get(evm, &api));
    api.timestamp(api.evm);
    TEARDOWN();
}

static void schedule(void * user_data, int64_t next_time) {
    (void) user_data;
    check_expected(next_time);
}

#define expect_schedule(next_time_) \
    expect_value(schedule, next_time, next_time_)

static void test_on_schedule(void **state) {
    SETUP();
    fbp_evm_register_schedule_callback(evm, schedule, NULL);
    expect_schedule(10);
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk1, NULL));
    assert_int_equal(2, fbp_evm_schedule(evm, 20, cbk1, NULL));
    expect_schedule(5);
    assert_int_equal(3, fbp_evm_schedule(evm, 5, cbk1, NULL));
    TEARDOWN();
}

#define expect_mutex() \
    expect_any(fbp_os_mutex_lock_, mutex); \
    expect_any(fbp_os_mutex_unlock_, mutex)


static void test_mutex(void **state) {
    SETUP();
    fbp_evm_register_mutex(evm, (fbp_os_mutex_t) evm);

    expect_mutex();
    assert_int_equal(1, fbp_evm_schedule(evm, 10, cbk1, NULL));
    expect_mutex();
    assert_int_equal(10, fbp_evm_interval_next(evm, 0));
    expect_mutex();
    assert_int_equal(10, fbp_evm_time_next(evm));
    expect_mutex();
    assert_int_equal(1, fbp_evm_scheduled_event_count(evm));
    expect_mutex();
    assert_int_equal(0, fbp_evm_process(evm, 0));
    expect_mutex();
    assert_int_equal(0, fbp_evm_cancel(evm, 1));
    expect_mutex();
    fbp_evm_register_schedule_callback(evm, NULL, NULL);

    expect_mutex();
    TEARDOWN();
}


int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_allocate),
            cmocka_unit_test(test_single_event),
            cmocka_unit_test(test_insert_two_events_in_order),
            cmocka_unit_test(test_insert_two_events_out_of_order),
            cmocka_unit_test(test_insert_two_events_and_cancel_first),
            cmocka_unit_test(test_api),
            cmocka_unit_test(test_on_schedule),
            cmocka_unit_test(test_mutex),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
