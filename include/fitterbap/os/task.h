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

/**
 * @file
 *
 * @brief OS Task abstraction.
 */

#ifndef FBP_OS_TASK_H__
#define FBP_OS_TASK_H__

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/config.h"
#include "fitterbap/time.h"

/**
 * @ingroup fbp_os
 * @defgroup fbp_os_task OS task abstraction
 *
 * @brief Provide a simple OS task abstraction.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief Get the identifier for the currently running task.
 *
 * @return The task ID.  If running single-threaded, return 0.
 */
FBP_API intptr_t fbp_os_current_task_id();


/**
 * @brief Sleep (pause thread execution) for a specified duration.
 *
 * @param duration The duration (see "fitterbap/time.h")
 */
FBP_API void fbp_os_sleep(int64_t duration);

/**
 * @brief Sleep (pause thread execution) for a specified duration.
 *
 * @param duration The duration in milliseconds.
 */
static inline void fbp_os_sleep_ms(int64_t duration_ms) {
    fbp_os_sleep(FBP_COUNTER_TO_TIME(duration_ms, 1000));
}

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_OS_MUTEX_H__ */
