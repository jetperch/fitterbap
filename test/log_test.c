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

#ifndef FBP_LOG_GLOBAL_LEVEL
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL
#endif

#ifndef FBP_LOG_LEVEL
#define FBP_LOG_LEVEL FBP_LOG_LEVEL_ALL
#endif

#define FBP_LOG_PRINTF(level, format, ...) \
    my_printf("%c " format "\n", fbp_log_level_char[level], __VA_ARGS__);

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdarg.h>
#include <stdio.h>
#include "fitterbap/log.h"


void my_printf(const char * format, ...) {
    char str[256];
    va_list arg;
    va_start(arg, format);
    vsnprintf(str, sizeof(str), format, arg);
    va_end(arg);
    check_expected_ptr(str);
}

static void test_logf(void **state) {
    (void) state;
    expect_string(my_printf, str, "C hello world\n");
    FBP_LOG_CRITICAL("%s %s", "hello", "world");
    expect_string(my_printf, str, "E hello world\n");
    FBP_LOG_ERROR("%s %s", "hello", "world");
    expect_string(my_printf, str, "W hello world\n");
    FBP_LOG_WARNING("%s %s", "hello", "world");
    expect_string(my_printf, str, "N hello world\n");
    FBP_LOG_NOTICE("%s %s", "hello", "world");
    expect_string(my_printf, str, "I hello world\n");
    FBP_LOG_INFO("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    FBP_LOG_DEBUG1("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    FBP_LOG_DEBUG2("%s %s", "hello", "world");
    expect_string(my_printf, str, "D hello world\n");
    FBP_LOG_DEBUG3("%s %s", "hello", "world");
}

static void test_logs(void **state) {
    (void) state;
    expect_string(my_printf, str, "C hello\n");
    FBP_LOG_CRITICAL("hello");
    expect_string(my_printf, str, "E hello\n");
    FBP_LOG_ERROR("hello");
    expect_string(my_printf, str, "W hello\n");
    FBP_LOG_WARNING("hello");
    expect_string(my_printf, str, "N hello\n");
    FBP_LOG_NOTICE("hello");
    expect_string(my_printf, str, "I hello\n");
    FBP_LOG_INFO("hello");
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG1("hello");
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG2("hello");
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG3("hello");
}


static void test_local_levels(void **state) {
    (void) state;
#undef FBP_LOG_LEVEL
#define FBP_LOG_LEVEL FBP_LOG_LEVEL_DEBUG
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG("%s", "hello");
    FBP_LOG_DEBUG2("%s", "hello");
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG("hello");
    FBP_LOG_DEBUG2("hello");
#undef FBP_LOG_LEVEL
#define FBP_LOG_LEVEL FBP_LOG_LEVEL_ALL
}

static void test_global_levels(void **state) {
    (void) state;
#undef FBP_LOG_GLOBAL_LEVEL
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_DEBUG
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG("%s", "hello");
    FBP_LOG_DEBUG2("%s", "hello");
    expect_string(my_printf, str, "D hello\n");
    FBP_LOG_DEBUG("hello");
    FBP_LOG_DEBUG2("hello");
#undef FBP_LOG_GLOBAL_LEVEL
#define FBP_LOG_GLOBAL_LEVEL FBP_LOG_LEVEL_ALL
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_logf),
            cmocka_unit_test(test_logs),
            cmocka_unit_test(test_local_levels),
            cmocka_unit_test(test_global_levels),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
