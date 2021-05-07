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
 * @brief Time synchronization.
 */

#ifndef FBP_COMM_TIMESYNC_H_
#define FBP_COMM_TIMESYNC_H_

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_ts Time Synchronization
 *
 * @brief A simple time synchronization protocol.
 *
 * This module implements a simple time synchronization method.
 *
 *
 *
 * ## References
 *
 * - [Precision Time Protocol](https://en.wikipedia.org/wiki/Precision_Time_Protocol)
 * - [Network Time Protocol](https://en.wikipedia.org/wiki/Network_Time_Protocol)
 * - [Cristian's Algorithm](https://en.wikipedia.org/wiki/Cristian%27s_algorithm)
 * - [Clock Synchronization](https://en.wikipedia.org/wiki/Clock_synchronization)
 */

FBP_CPP_GUARD_START

/**
 * @brief Get the platform-specific counter value.
 *
 * @return The counter value, which rolls over.
 *
 * The counter frequency should be at least 1000 Hz (1 ms period).
 * Higher frequency allows for increased accuracy, both for time
 * convergence and individual samples.  Most applications should
 * have a frequency around 1,000,000 Hz (1 MHz).
 */
FBP_API uint32_t fbp_ts_platform_counter();

/**
 * @brief Get the current time.
 *
 * @return The current time.  The format is 34Q30 as defined by
 *      fitterbap/time.h.
 *
 * When using this module to provide time, edit your fbp/config.h file
 * to have:
 * static inline int64_t fbp_time_get() { return fbp_ts_time(); }
 */
FBP_API int64_t fbp_ts_time();

/**
 * @brief Update the current time with new data.
 *
 * @param src_tx The time we sent the TIMESYNC request message.
 * @param tgt_rx The time that the target received the TIMESYNC request message.
 * @param tgt_tx The time that the target send the TIMESYNC response message.
 *
 * During normal operation, call this function at least 1/2 of the
 * platform counter rollover duration.
 */
FBP_API void fbp_ts_update(int64_t src_tx, int64_t tgt_rx, int64_t tgt_tx);

/**
 * @brief Update the time using the platform counter.
 *
 * During normal operation, call fbp_ts_update() at least 1/2 of the
 * platform counter rollover duration.  When this device is disconnected,
 * the system cannot call fbp_ts_update().  Call this function
 * instead.
 */
FBP_API void fbp_ts_process();

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_TIMESYNC_H_ */
