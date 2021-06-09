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
#include "fitterbap/time.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_ts Time Synchronization
 *
 * @brief A simple time synchronization protocol.
 *
 * This module implements a simple time synchronization method using
 * the client's fbp_time_counter() and the server-provide UTC time.
 *
 * The server is always considered as time source truth.  If an
 * implementation uses multiple servers, then only one server
 * connection should be connected to update UTC time.
 *
 * ## References
 *
 * - [Precision Time Protocol](https://en.wikipedia.org/wiki/Precision_Time_Protocol)
 * - [Network Time Protocol](https://en.wikipedia.org/wiki/Network_Time_Protocol)
 * - [Cristian's Algorithm](https://en.wikipedia.org/wiki/Cristian%27s_algorithm)
 * - [Clock Synchronization](https://en.wikipedia.org/wiki/Clock_synchronization)
 */

FBP_CPP_GUARD_START


/// Opaque instance
struct fbp_ts_s;


/**
 * @brief The current local, monotonic counter value.
 *
 * @return The counter value.
 */
static inline uint64_t fbp_ts_counter() {
    return fbp_time_counter().value;
}

/**
 * @brief Get the current time.
 *
 * @param The timesync instance.  NULL will use the time provided by
 *      the first created instance, if it exists.  This behavior enables a
 *      simple singleton interface, which is common for most applications.
 * @return The current time.  The format is 34Q30 as defined by
 *      fitterbap/time.h.  This function returns 0 if the UTC
 *      time is not yet known.
 * @see fbp_time_utc
 *
 * When using this module to provide time, edit your fbp/config.h file
 * to define FBP_TS_PROVIDE_UTC.
 *
 * This function is thread-safe.
 */
FBP_API int64_t fbp_ts_time(struct fbp_ts_s * self);

/**
 * @brief Update the timesync instance with new measurement data.
 *
 * @param self The instance, or NULL to ignore.
 * @param src_tx The time we sent the TIMESYNC request message, in
 *      fbp_ts_counter() ticks.
 * @param tgt_rx The time that the target received the TIMESYNC request
 *      message, in the server's UTC time.
 * @param tgt_tx The time that the target send the TIMESYNC response message,
 *      in the server's UTC time.
 * @param src_rx The time we received the TIMESYNC request message, in
 *      fbp_ts_counter() ticks.
 */
FBP_API void fbp_ts_update(struct fbp_ts_s * self, uint64_t src_tx, int64_t tgt_rx, int64_t tgt_tx, uint64_t src_rx);

/**
 * @brief Allocate and initialize a new instance.
 *
 * @return The new instance.
 */
FBP_API struct fbp_ts_s * fbp_ts_initialize();

/**
 * @brief Finalize and free an existing instance.
 *
 * @param self The instance created by fbp_ts_initialize().
 *
 * Most embedded systems should never use this function.
 */
FBP_API void fbp_ts_finalize(struct fbp_ts_s * self);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_TIMESYNC_H_ */
