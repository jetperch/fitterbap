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
 * @brief Log handler.
 */

#ifndef FBP_LOG_HANDLER_H_
#define FBP_LOG_HANDLER_H_

#include "fitterbap/log.h"
#include <stdint.h>

/**
 * @ingroup fbp
 * @defgroup fbp_logh Log handler
 *
 * @brief Format, queue, and dispatch log messages.
 *
 * @{
 */

FBP_CPP_GUARD_START

#ifndef FBP_LOGH_FILENAME_SIZE_MAX
/// The filename maximum size, including the null terminator character.
#define FBP_LOGH_FILENAME_SIZE_MAX (32)
#endif

#ifndef FBP_LOGH_MESSAGE_SIZE_MAX
/// The log message maximum size, including the null terminator character.
#define FBP_LOGH_MESSAGE_SIZE_MAX (80)
#endif

#ifndef FBP_LOGH_DISPATCH_MAX
/// The maximum number of dispatch functions that can be registered.
#define FBP_LOGH_DISPATCH_MAX (4)
#endif

/// The record format version.
#define FBP_LOGH_VERSION  (1)

/**
 * @brief The log record header format.
 *
 * This header is also used directly by the comm/logp.h implementation.
 */
struct fbp_logh_header_s {
    /// The FBP_LOGH_VERSION log message format major version.
    uint8_t version;
    /// The fbp_log_level_e.
    uint8_t level;
    /// The prefix origin character, usually the same as PubSub.
    char origin_prefix;
    /// The originating thread id or hash, or 0 if unused.
    uint8_t origin_thread;
    /// The originating file line number number.
    uint32_t line;
    /// The Fitterbap UTC time.
    uint64_t timestamp;
};

/// The opaque instance.
struct fbp_logh_s;

/**
 * @brief Receive a log message.
 *
 * @param user_data The arbitrary user data.
 * @param header The log record header.
 * @param filename The log record filename.
 * @param message The log message.
 *
 * @return 0 or FBP_ERROR_FULL to try again later.
 * @see fbp_logh_dispatch_register().
 *
 * All parameters only remain valid for the duration of the call.
 * The caller retains ownership.
 */
typedef int32_t (*fbp_logh_recv)(void * user_data, struct fbp_logh_header_s const * header,
        const char * filename, const char * message);

/**
 * @brief Function called for each publish.
 *
 * @param user_data The arbitrary user data.
 * @see fbp_logh_publish()
 * @see fbp_logh_publish_record()
 */
typedef void (*fbp_logh_on_publish)(void * user_data);

/**
 * @brief Publish a new log message.
 *
 * @param self The instance or NULL to use the default singleton.
 * @param level The logging level.
 * @param filename The source filename.
 * @param line The source line number.
 * @param format The formatting string for the arguments.
 * @param ... The arguments to format.
 * @return 0 or FBP_ERROR_UNAVAILABLE, FBP_ERROR_FULL.
 */
FBP_API int32_t fbp_logh_publish(struct fbp_logh_s * self, uint8_t level, const char * filename, uint32_t line, const char * format, ...);

/**
 * @brief Publish a new log message.
 *
 * @param self The instance or NULL to use the default singleton.
 * @param header The log header, which is forwarded exactly.
 * @param filename The filename that generated this log message.
 * @param message The log message.
 * @return 0 or FBP_ERROR_NOT_AVAILABLE, FBP_ERROR_FULL.
 */
FBP_API int32_t fbp_logh_publish_formatted(struct fbp_logh_s * self, struct fbp_logh_header_s const * header,
                                           const char * filename, const char * message);

/**
 * @brief Register a callback for log message dispatch.
 *
 * @param self The instance or NULL to use the default singleton.
 * @param fn A function to call on a received log message.
 *      This function will be called from within fbp_logh_process().
 * @param user_data The arbitrary user data for fn.
 * @return 0 or FBP_ERROR_FULL.
 */
FBP_API int32_t fbp_logh_dispatch_register(struct fbp_logh_s * self, fbp_logh_recv fn, void * user_data);

/**
 * @brief Unregister a callback.
 *
 * @param self The instance or NULL to use the default singleton.
 * @param fn The function previous registered fbp_logh_dispatch_register().
 * @param user_data The same arbitrary data provided to fbp_logh_dispatch_register().
 * @return 0 or FBP_ERROR_NOT_FOUND.
 */
FBP_API int32_t fbp_logh_dispatch_unregister(struct fbp_logh_s * self, fbp_logh_recv fn, void * user_data);

/**
 * @brief Unregister all callbacks.
 *
 * @param self The instance or NULL to use the default singleton.
 * @return 0 or FBP_ERROR_NOT_FOUND.
 *
 * Prefer to use fbp_logh_dispatch_unregister() so that registered callbacks
 * remain in control.  The anticipated use case for this function is during
 * fault handling to process all outstanding messages to a debug UART.
 */
FBP_API void fbp_logh_dispatch_unregister_all(struct fbp_logh_s * self);

/**
 * @brief Register a function to call on publish.
 *
 * @param self The instance or NULL to use the default singleton.
 * @param fn The function called when a log message is published
 *      or NULL to disable this feature.
 *      This callback function allows publish to notify the log thread
 *      that a new message is available.  Normally, this function will
 *      set a thread notification or event, which the thread waits on.
 * @param user_data The arbitrary data for on_publish_fn.
 */
FBP_API void fbp_logh_publish_register(struct fbp_logh_s * self, fbp_logh_on_publish fn, void * user_data);

/**
 * @brief Process all available log messages.
 *
 * @param self The instance.
 * @return 0 if all message are processed or error code.
 *
 * This function is normally called from a dedicated logging thread.
 */
FBP_API int32_t fbp_logh_process(struct fbp_logh_s * self);

/**
 * @brief Allocate and initialize a new log handler instance.
 *
 * @param origin_prefix The origin prefix character for this instance.
 * @param msg_buffers_max The maximum number of allowed message buffers.
 *      If this instance runs out of message buffers, it will drop
 *      log messages until the queue empties.
 * @param time_fn The function that provides utc time.  NULL will
 *      use fbp_time_utc().
 * @return The log hander instance.
 */
FBP_API struct fbp_logh_s * fbp_logh_initialize(char origin_prefix, uint32_t msg_buffers_max,
                                                int64_t (*time_fn)());

/**
 * @brief Finalize and deallocate a log handler instance.
 *
 * @param self The instance returned by fbp_logh_initialize().
 */
FBP_API void fbp_logh_finalize(struct fbp_logh_s * self);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_LOG_HANDLER_H_ */
