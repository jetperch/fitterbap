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

/**
 * @file
 *
 * @brief MS Windows platform
 */

#ifndef FBP_HOST_WIN_PLATFORM_H_
#define FBP_HOST_WIN_PLATFORM_H_

#include "fitterbap/platform_dependencies.h"
#include "fitterbap/assert.h"
#include "fitterbap/log.h"
#include <string.h>  // for memset, memcpy

void * fbp_alloc_(fbp_size_t size_bytes);
void fbp_free_(void * ptr);
uint32_t fbp_time_counter_frequency_();
uint64_t fbp_time_counter_u64_();
uint32_t fbp_time_counter_u32_();
int64_t fbp_time_utc_();
fbp_os_mutex_t fbp_os_mutex_alloc_();
void fbp_os_mutex_free_(fbp_os_mutex_t mutex);
void fbp_os_mutex_lock_(fbp_os_mutex_t mutex);
void fbp_os_mutex_unlock_(fbp_os_mutex_t mutex);
intptr_t fbp_os_current_task_id_();
void fbp_os_sleep_(int64_t duration);
void fbp_log_printf_(const char * format, ...) FBP_PRINTF_FORMAT;

FBP_INLINE_FN uint32_t fbp_clz(uint32_t x) {
    return fbp_clz_generic(x);
}

FBP_INLINE_FN uint32_t fbp_upper_power_of_two(uint32_t x) {
    return fbp_upper_power_of_two_generic(x);
}

FBP_INLINE_FN void fbp_memset(void * ptr, int value, fbp_size_t num) {
    memset(ptr, value, num);
}

FBP_INLINE_FN void fbp_memcpy(void * destination, void const * source, fbp_size_t num) {
    memcpy(destination, (void *) source, num);
}

FBP_INLINE_FN void * fbp_alloc(fbp_size_t size_bytes) {
    void * ptr = fbp_alloc_(size_bytes);
    FBP_ASSERT_ALLOC(ptr);
    return ptr;
}

FBP_INLINE_FN void fbp_free(void * ptr) {
    fbp_free_(ptr);
}

FBP_INLINE_FN uint32_t fbp_time_counter_frequency() {
    return fbp_time_counter_frequency_();
}

FBP_INLINE_FN uint64_t fbp_time_counter_u64() {
    return fbp_time_counter_u64_();
}

FBP_INLINE_FN uint32_t fbp_time_counter_u32() {
    return (uint32_t) fbp_time_counter_u32_();
}

FBP_INLINE_FN int64_t fbp_time_utc() {
    return fbp_time_utc_();
}

FBP_INLINE_FN fbp_os_mutex_t fbp_os_mutex_alloc() {
    return fbp_os_mutex_alloc_();
}

FBP_INLINE_FN void fbp_os_mutex_free(fbp_os_mutex_t mutex) {
    fbp_os_mutex_free_(mutex);
}

FBP_INLINE_FN void fbp_os_mutex_lock(fbp_os_mutex_t mutex) {
    fbp_os_mutex_lock_(mutex);
}

FBP_INLINE_FN void fbp_os_mutex_unlock(fbp_os_mutex_t mutex) {
    fbp_os_mutex_unlock_(mutex);
}

FBP_INLINE_FN intptr_t fbp_os_current_task_id() {
    return fbp_os_current_task_id_();
}

FBP_INLINE_FN void fbp_os_sleep(int64_t duration) {
    fbp_os_sleep_(duration);
}

#ifndef FBP_LOG_PRINTF
#define FBP_LOG_PRINTF(level, format, ...) fbp_log_printf_("%c %s:%d: " format "\n", fbp_log_level_char[level], __FILENAME__, __LINE__, __VA_ARGS__);
#endif

#endif /* FBP_HOST_WIN_PLATFORM_H_ */
