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
 * @brief OS Mutex abstraction.
 */

#ifndef FBP_OS_MUTEX_H__
#define FBP_OS_MUTEX_H__

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/config.h"

/**
 * @ingroup fbp_os
 * @defgroup fbp_os_mutex OS Mutex abstraction
 *
 * @brief Provide a simple OS mutex abstraction.
 *
 * Although FBP attempts to avoid OS calls, mutexes occur
 * frequently enough that the FBP library standardizes
 * on a convention.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief Allocate a new mutex.
 *
 * @return The mutex or 0.
 */
FBP_API fbp_os_mutex_t fbp_os_mutex_alloc();

/**
 * @brief Free an existing mutex (not recommended).
 *
 * @param mutex The mutex to free, previous produced using fbp_os_mutex_alloc().
 */
FBP_API void fbp_os_mutex_free(fbp_os_mutex_t mutex);

/**
 * @brief Lock a mutex.
 *
 * @param mutex The mutex to lock.  If NULL, then skip the lock.
 *
 * Be sure to call fbp_os_mutex_unlock() when done.
 *
 * This function will use the default platform timeout.
 * An lock that takes longer than the timeout indicates
 * a system failure.  In deployed embedded systems, this
 * should trip the watchdog timer.
 */
FBP_API void fbp_os_mutex_lock(fbp_os_mutex_t mutex);

/**
 * @brief Unlock a mutex.
 *
 * @param mutex The mutex to unlock, which was previously locked
 *      with fbp_os_mutex_lock().  If NULL, then skip the unlock.
 */
FBP_API void fbp_os_mutex_unlock(fbp_os_mutex_t mutex);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_OS_MUTEX_H__ */
