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

#include "fitterbap/assert.h"
#include "fitterbap/log.h"
#include "fitterbap/lib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static fbp_lib_fatal_fn fatal_fn_ = 0;
static void * fatal_user_data_ = 0;
static fbp_lib_print_fn print_fn_ = 0;
static void * print_user_data_ = 0;

static void fbp_lib_printf_(const char *format, ...);

void fbp_lib_initialize() {
    fbp_allocator_set((fbp_alloc_fn) malloc, (fbp_free_fn) free);
}

void fbp_lib_fatal_set(fbp_lib_fatal_fn fn, void * user_data) {
    fatal_fn_ = fn;
    fatal_user_data_ = user_data;
}

void fbp_lib_print_set(fbp_lib_print_fn fn, void * user_data) {
    print_fn_ = fn;
    print_user_data_ = user_data;
    fbp_log_initialize(fbp_lib_printf_);
}

static void fbp_lib_printf_(const char *format, ...) {
    char buffer[8192];
    va_list arg;
    va_start(arg, format);
    if (print_fn_) {
        vsnprintf(buffer, sizeof(buffer), format, arg);
        print_fn_(print_user_data_, buffer);
    } else {
        vprintf(format, arg);
    }
    va_end(arg);
}

void fbp_fatal(char const * file, int line, char const * msg) {
    if (fatal_fn_) {
        fatal_fn_(fatal_user_data_, file, line, msg);
    } else {
        fbp_lib_printf_("FATAL: %s : %d : %s\n", file, line, msg);
    }
}

FBP_API void * fbp_lib_alloc(fbp_size_t sz) {
    return fbp_alloc_clr(sz);
}

FBP_API void fbp_lib_free(void * ptr) {
    if (ptr) {
        fbp_free(ptr);
    }
}
