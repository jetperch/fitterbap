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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE

#include "fitterbap/common_header.h"
#include <windows.h>

uint32_t fbp_time_counter_frequency_() {
    LARGE_INTEGER perf_frequency;
    QueryPerformanceFrequency(&perf_frequency);
    FBP_ASSERT(perf_frequency.QuadPart < UINT32_MAX);
    return (uint32_t) perf_frequency.QuadPart;
}

uint64_t fbp_time_counter_u64_() {
    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    LARGE_INTEGER perf_counter;
    QueryPerformanceCounter(&perf_counter);
    return perf_counter.QuadPart;
}

uint32_t fbp_time_counter_u32_() {
    return (uint32_t) fbp_time_counter_u64();
}

int64_t fbp_time_utc_() {
    // Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
    // python
    // import dateutil.parser
    // dateutil.parser.parse('2018-01-01T00:00:00Z').timestamp() - dateutil.parser.parse('1601-01-01T00:00:00Z').timestamp()
    static const int64_t offset_s = 131592384000000000LL;  // 100 ns
    static const uint64_t frequency = 10000000; // 100 ns
    FILETIME filetime;
    GetSystemTimePreciseAsFileTime(&filetime);
    uint64_t t = ((uint64_t) filetime.dwLowDateTime) | (((uint64_t) filetime.dwHighDateTime) << 32);
    t -= offset_s;
    return FBP_COUNTER_TO_TIME(t, frequency);
}

fbp_os_mutex_t fbp_os_mutex_alloc_() {
    fbp_os_mutex_t mutex = CreateMutex(
            NULL,                   // default security attributes
            FALSE,                  // initially not owned
            NULL);                  // unnamed mutex
            FBP_ASSERT_ALLOC(mutex);
            return mutex;
}

void fbp_os_mutex_free_(fbp_os_mutex_t mutex) {
    if (mutex) {
        CloseHandle(mutex);
    }
}

void fbp_os_mutex_lock_(fbp_os_mutex_t mutex) {
    if (mutex) {
        DWORD rc = WaitForSingleObject(mutex, FBP_CONFIG_OS_MUTEX_LOCK_TIMEOUT_MS);
        if (WAIT_OBJECT_0 != rc) {
            FBP_LOG_CRITICAL("mutex lock failed: %d", rc);
            FBP_FATAL("mutex lock failed");
        }
    } else {
        FBP_LOGD1("lock, but mutex is null");
    }
}

void fbp_os_mutex_unlock_(fbp_os_mutex_t mutex) {
    if (mutex) {
        if (!ReleaseMutex(mutex)) {
            FBP_LOG_CRITICAL("mutex unlock failed");
            FBP_FATAL("mutex unlock failed");
        }
    } else {
        FBP_LOGD1("unlock, but mutex is null");
    }
}

intptr_t fbp_os_current_task_id_() {
    return ((intptr_t) GetCurrentThreadId());
}

void fbp_os_sleep_(int64_t duration) {
    if (duration < 0) {
        return;
    }
    int64_t duration_ms = FBP_TIME_TO_MILLISECONDS(duration);
    if (duration_ms > 0xffffffff) {
        duration_ms = 0xffffffff;
    }
    Sleep((DWORD) FBP_TIME_TO_MILLISECONDS(duration_ms));
}

