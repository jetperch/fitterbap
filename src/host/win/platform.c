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

#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include <windows.h>


struct fbp_time_counter_s fbp_time_counter() {
    struct fbp_time_counter_s counter;
    static bool first = true;
    static uint64_t offset = 0;     // in 34Q30 time
    static LARGE_INTEGER perf_frequency = {.QuadPart = 0};
    // millisecond counter
    //SYSTEMTIME time;
    //GetSystemTime(&time);
    //return (time.wSecond * 1000) + time.wMilliseconds;

    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    LARGE_INTEGER perf_counter;

    QueryPerformanceCounter(&perf_counter);

    if (first) {
        QueryPerformanceFrequency(&perf_frequency);
        offset = perf_counter.QuadPart;
        first = false;
    }

    counter.value = perf_counter.QuadPart - offset;
    counter.frequency = perf_frequency.QuadPart;
    return counter;
}

FBP_API int64_t fbp_time_utc() {
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
