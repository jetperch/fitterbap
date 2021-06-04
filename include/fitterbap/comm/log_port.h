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
 * @brief Log message port definitions.
 */

#ifndef FBP_COMM_LOG_PORT_H_
#define FBP_COMM_LOG_PORT_H_

#include "fitterbap/config.h"
#include "fitterbap/comm/port.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_log_port Log Port
 *
 * @brief The log port used to convey system log messages.
 *
 * The log message format is:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">version[7:0]</td></tr>
 *  <tr><td colspan="8">origin_prefix</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">length[15:8]</td></tr>
 *  <tr>
 *      <td colspan="4">length[19:16]</td>
 *      <td colspan="3">level[3:0]</td>
 *  </tr>
 *  <tr><td colspan="8">origin_thread[7:0]</td></tr>
 *  <tr><td colspan="8">rsv8_1[7:0]</td></tr>
 *  <tr><td colspan="8">rsv8_2[15:8]</td></tr>
 *  <tr><td colspan="8">timestamp[7:0]</td></tr>
 *  <tr><td colspan="8">timestamp[15:8]</td></tr>
 *  <tr><td colspan="8">timestamp[23:16]</td></tr>
 *  <tr><td colspan="8">timestamp[31:24]</td></tr>
 *  <tr><td colspan="8">timestamp[39:32]</td></tr>
 *  <tr><td colspan="8">timestamp[47:40]</td></tr>
 *  <tr><td colspan="8">timestamp[55:48]</td></tr>
 *  <tr><td colspan="8">timestamp[63:56]</td></tr>
 *  <tr><td colspan="8">filename</td></tr>
 *  <tr><td colspan="8">...</td></tr>
 *  <tr><td colspan="8">0x1f</td></tr>
 *  <tr><td colspan="8">message</td></tr>
 *  <tr><td colspan="8">...</td></tr>
 *  <tr><td colspan="8">0</td></tr>
 * </table>
 *
 * @{
 */

FBP_CPP_GUARD_START

#ifndef FBP_LOGP_FILENAME_SIZE_MAX
#define FBP_LOGP_FILENAME_SIZE_MAX (30)
#endif

#ifndef FBP_LOGP_MESSAGE_SIZE_MAX
#define FBP_LOGP_MESSAGE_SIZE_MAX (80)
#endif

#define FBP_LOGP_DATA_SIZE_MAX (FBP_LOGP_FILENAME_SIZE_MAX + 1 + FBP_LOGP_MESSAGE_SIZE_MAX + 1)
#define FBP_LOGP_SEP '\x1f'
#define FBP_LOGP_VERSION  (1)

/**
 * @brief The log record.
 */
struct fbp_logp_record_s {
    /// The Fitterbap UTC time.
    uint64_t timestamp;
    /// The fbp_log_level_e in the lower 4 bits.
    uint8_t level;
    /// The prefix origin character, usually the same as PubSub.
    uint8_t origin_prefix;
    /// The originating thread id or hash, or 0 if unused.
    uint8_t origin_thread;
    /// The originating file line number number.
    uint32_t line;
    /// The filename
    char * filename;
    /// The message
    char * message;
};

/**
 * @brief The log message header structure.
 */
struct fbp_logp_header_s {
    /// The log message format major version, currently 1.
    uint8_t version;
    /// The prefix origin character, usually the same as PubSub.
    uint8_t origin_prefix;
    /// The originating file line number number.
    uint16_t line;
    /// The fbp_log_level_e in the lower 4 bits.
    uint8_t level;
    /// The originating thread id or hash, or 0 if unused.
    uint8_t origin_thread;
    /// Reserved for future use.  Set to 0.
    uint8_t rsv8_1;
    /// Reserved for future use.  Set to 0.
    uint8_t rsv8_2;
    /// The Fitterbap UTC time.
    uint64_t timestamp;
};

/**
 * @brief The full-sized logging message structure.
 */
struct fbp_logp_msg_buffer_s {
    /// The message metadata.
    struct fbp_logp_header_s header;
    /// The filename and message strings.
    char data[FBP_LOGP_DATA_SIZE_MAX];
};

/**
 * @brief The log port configuration.
 *
 * @see fbp_logp_factory
 */
struct fbp_logp_config_s {
    /**
     * @brief The maximum number of allowed message buffers.
     *
     * If this instance runs out of message buffers, it will drop
     * log messages until the queue empties.
     */
    uint32_t msg_buffers_max;

    /**
     * @brief The function called when a log message is published.
     *
     * @param user_data The arbitrary user data.
     *
     * This callback function allows publish to notify the log thread
     * that a new message is available.  Normally, this function will
     * set a thread notification or event, which the thread waits on.
     */
    void (*on_publish_fn)(void * user_data);

    /// Arbitrary data for on_publish_fn.
    void * on_publish_user_data;

    /**
     * @brief The mutex instance.
     *
     * This instance is normally run in its own thread.  This mutex allows
     * other threads to safely publish log messages.
     */
    fbp_os_mutex_t mutex;
};

/**
 * @brief Receive a log message.
 *
 * @param user_data The arbitrary user data provided
 *      to fbp_logp_recv_register().
 * @param record The log record, which only remains valid for the duration
 *      of this function call.  The caller retains ownership.
 */
typedef void (*fbp_logp_on_recv)(void * user_data, struct fbp_logp_record_s const * record);

/**
 * @brief Publish a new log entry.
 *
 * @param api The instance
 * @param level The logging level.
 * @param filename The source filename.
 * @param line The source line number.
 * @param format The formatting string for the arguments.
 * @param ... The arguments to format.
 * @return 0 or FBP_ERROR_UNAVAILABLE, FBP_ERROR_FULL.
 */
FBP_API int32_t fbp_logp_publish(struct fbp_port_api_s * api, uint8_t level, const char * filename, uint32_t line, const char * format, ...);

/**
 * @brief Publish a new log message.
 *
 * @param api The instance.
 * @param record The log record.
 * @return 0 or FBP_ERROR_NOT_AVAILABLE, FBP_ERROR_FULL.
 */
FBP_API int32_t fbp_logp_publish_record(struct fbp_port_api_s * api, struct fbp_logp_record_s * record);

/**
 * @brief Register a message receive callback.
 *
 * @param api The instance.
 * @param cbk_fn The function to call on received message, which is called
 *      from within fbp_logp_process().  Provide NULL to unregister.
 * @param cbk_user_data The arbitrary user data for cbk_fn.
 */
FBP_API void fbp_logp_recv_register(struct fbp_port_api_s * api, fbp_logp_on_recv cbk_fn, void * cbk_user_data);

/**
 * @brief Process all available log messages.
 *
 * @param api The instance.
 * @return 0 if all message are processed or error code.
 *
 * This function is normally called from a dedicated logging thread.
 */
FBP_API int32_t fbp_logp_process(struct fbp_port_api_s * api);

/**
 * @brief Construct a new log port instance.
 *
 * @param msg_buffers_max The maximum number of allowed message buffers.
 *      If this instance runs out of message buffers, it will drop
 *      log messages until the queue empties.
 * @return The port instance for logging.
 *
 * Populate the topic_prefix, transport, and port_id fields before
 * calling initialize.  This implementation does not use pubsub.
 *
 * If this port is expected to handle incoming log messages, call
 * fbp_logp_recv_register().
 */
FBP_API struct fbp_port_api_s * fbp_logp_factory(struct fbp_logp_config_s const * config);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_LOG_PORT_H_ */
