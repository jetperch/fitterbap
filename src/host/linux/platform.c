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
#include <time.h>

static int64_t counter_base_seconds_ = 0;
static uint64_t counter_frequency_ = 0;


struct fbp_time_counter_s fbp_time_counter() {
    struct fbp_time_counter_s counter;
    struct timespec ts;
    struct timespec ts_res;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (counter_frequency_ == 0) {
        clock_getres(CLOCK_MONOTONIC, &ts_res);
        if (ts_res.tv_sec > 0) {
            // really!  too slow.
        }
        counter_frequency_ = 1000000000 / ts_res.tv_nsec;
        counter_base_seconds_ = (int64_t) ts.tv_sec;
    }
    counter.frequency = counter_frequency_;
    counter.value = FBP_TIME_SECOND * ((int64_t) ts.tv_sec - counter_base_seconds_);
    counter.value += FBP_NANOSECONDS_TO_TIME(ts.tv_nsec);
    return counter;
}

FBP_API int64_t fbp_time_utc() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int64_t t = FBP_TIME_SECOND * (int64_t) ts.tv_sec;
    t += FBP_NANOSECONDS_TO_TIME(ts.tv_nsec);
    return t;
}
