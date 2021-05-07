/*
 * Copyright 2017-2021 Jetperch LLC
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
 * @brief
 *
 * Time-based event manager.
 */

#ifndef FBP_EVENT_MANAGER_H_
#define FBP_EVENT_MANAGER_H_

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/os/mutex.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_evm Event manager
 *
 * @brief Event manager.
 *
 * This module contains an event manager for scheduling, cancelling, and
 * processing events based upon the passage of time.  This module is intended
 * to allow event scheduling within a single thread.  It provides an optional
 * locking mechanism through fbp_evm_register_mutex() to support accesses
 * from multiple threads.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The opaque event manager instance.
struct fbp_evm_s;

/**
 * @brief The function prototype called when an event completes.
 *
 * @param user_data The arbitrary user data.
 * @param event_id The event that completed.
 */
typedef void (*fbp_evm_callback)(void * user_data, int32_t event_id);

/**
 * @brief Get the current time in FBP 34Q30 format.
 *
 * @param hal The HAL instance (user_data).
 * @return The current time.
 *      Usually wraps one of:
 *      - fbp_time_rel(): monotonic relative to platform start.
 *      - fbp_time_utc(): most accurate to real wall clock time, but may jump.
 */
typedef int64_t (*fbp_evm_timestamp_fn)(struct fbp_evm_s * evm);

/**
 * @brief Schedule a new event.
 *
 * @param evm The event manager instance.
 * @param timestamp The timestamp for when the event should occur.  If <= 0,
 *      then ignore and return 0.
 * @param cbk_fn The function to call at timestamp time.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return The event_id which is provided to the callback.  The event_id
 *      can also be used to cancel the event with event_cancel.
 *      On error, return 0.
 */
typedef int32_t (*fbp_evm_schedule_fn)(struct fbp_evm_s * evm, int64_t timestamp,
                    fbp_evm_callback cbk_fn, void * cbk_user_data);

/**
 * @brief Cancel a pending event.
 *
 * @param evm The event manager instance.
 * @param event_id The event_id returned by event_schedule().  If 0, ignore.
 * @return 0 or error code.
 */
typedef int32_t (*fbp_evm_cancel_fn)(struct fbp_evm_s * evm, int32_t event_id);

/**
 * @brief An abstract scheduler interface.
 *
 * This interface allows multiple clients (different modules)
 * to easily share the same scheduler.  The processing-related functions
 * are intentionally omitted, since only the main module (thread)
 * calls them.
 */
struct fbp_evm_api_s {
    struct fbp_evm_s * evm;             ///< The event manager instance.
    fbp_evm_timestamp_fn timestamp;     ///< The timestamp function, wrap fbp_time_rel() by default.
    fbp_evm_schedule_fn schedule;       ///< The schedule function, fbp_evm_schedule().
    fbp_evm_cancel_fn cancel;           ///< The cancel function, fbp_evm_cancel().
};

/**
 * Allocate a new event manager instance.
 *
 * @return The new event manager instance.
 *
 * Use fbp_evm_free() to free the instance when done.
 */
FBP_API struct fbp_evm_s * fbp_evm_allocate();

/**
 * @brief Free an instance previously allocated by fbp_evm_allocate.
 *
 * @param self The event manager instance previous returned by fbp_evm_allocate().
 */
FBP_API void fbp_evm_free(struct fbp_evm_s * self);

/**
 * @brief Schedule a new event.
 *
 * @param self The event manager instance.
 * @param timestamp The timestamp for when the event should occur.  If <= 0,
 *      then ignore and return 0.
 * @param cbk_fn The function to call at timestsmp time.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return The event_id which is provided to the callback.  The event_id
 *      can also be used to cancel the event with event_cancel.
 *      On error, return 0.
 */
FBP_API int32_t fbp_evm_schedule(struct fbp_evm_s * self, int64_t timestamp,
                                   fbp_evm_callback cbk_fn, void * cbk_user_data);

/**
 * @brief Cancel a pending event.
 *
 * @param self The event manager instance.
 * @param event_id The event_id returned by event_schedule().  If 0, ignore.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_evm_cancel(struct fbp_evm_s * self, int32_t event_id);

/**
 * @brief The time for the next scheduled event.
 *
 * @param self The event manager instance.
 * @return The time for the next scheduled event.  If no events are
 *      currently pending, returns FBP_TIME_MIN.
 */
FBP_API int64_t fbp_evm_time_next(struct fbp_evm_s * self);

/**
 * @brief The time remaining until the next scheduled event.
 *
 * @param self The event manager instance.
 * @param time_current The current time.
 * @return The interval until the next scheduled event.  If no events are
 *      currently pending, returns -1.
 */
FBP_API int64_t fbp_evm_interval_next(struct fbp_evm_s * self, int64_t time_current);

/**
 * @brief Process all pending events.
 *
 * @param self The event manager instance.
 * @param time_current The current time.  Any event schedule at or before this
 *      time will be processed.  The definition of the time is left to the caller,
 *      but must be shared by all clients.  Commonly selected times are:
 *      - fbp_time_rel(): monotonic relative to platform start
 *      - fbp_time_utc(): most accurate to real wall clock time, but may jump.
 * @return The total number of events processed.
 */
FBP_API int32_t fbp_evm_process(struct fbp_evm_s * self, int64_t time_current);

/**
 * @brief Register a mutex to support blocking multi-threaded operation.
 *
 * @param self The event manager instance.
 * @param mutex The mutex instance.
 */
void fbp_evm_register_mutex(struct fbp_evm_s * self, fbp_os_mutex_t mutex);

/**
 * @brief Populate the API.
 *
 * @param self The event manager instance.
 * @param api The API instance to populate with the default functions.
 * @return 0 or error code.
 *
 * Use fbp_time_rel for api->timestamp by default.
 */
FBP_API int32_t fbp_evm_api_get(struct fbp_evm_s * self, struct fbp_evm_api_s * api);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_EVENT_MANAGER_H_ */
