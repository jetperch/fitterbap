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

#include "fitterbap/log.h"
#include <stdio.h>


char const * const fbp_log_level_str[FBP_LOG_LEVEL_ALL + 1] = {
        "EMERGENCY",
        "ALERT",
        "CRITICAL",
        "ERROR",
        "WARN",
        "NOTICE"
        "INFO",
        "DEBUG",
        "DEBUG2"
        "DEBUG3",
        "ALL"
};

char const fbp_log_level_char[FBP_LOG_LEVEL_ALL + 1] = {
        '!', 'A', 'C', 'E', 'W', 'N', 'I', 'D', 'D', 'D', '.'
};


void fbp_log_printf_default(const char * fmt, ...) {
    (void) fmt;
}

volatile fbp_log_printf FBP_USED fbp_log_printf_ = fbp_log_printf_default;

int fbp_log_initialize(fbp_log_printf handler) {
    if (NULL == handler) {
        fbp_log_printf_ = fbp_log_printf_default;
    } else {
        fbp_log_printf_ = handler;
    }
    return 0;
}

void fbp_log_finalize() {
    fbp_log_initialize(0);
}
