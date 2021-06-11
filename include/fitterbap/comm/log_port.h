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
 * @brief Log message port.
 */

#ifndef FBP_COMM_LOG_PORT_H_
#define FBP_COMM_LOG_PORT_H_

#include "fitterbap/config.h"
#include "fitterbap/comm/port.h"
#include "fitterbap/logh.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_log_port Log Port
 *
 * @brief The log port used to convey log messages to/from the log handler.
 *
 * The log message format is:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">version[7:0]</td></tr>
 *  <tr><td colspan="8">level</td></tr>
 *  <tr><td colspan="8">origin_prefix</td></tr>
 *  <tr><td colspan="8">origin_thread[7:0]</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">length[15:8]</td></tr>
 *  <tr><td colspan="8">length[23:16]</td></tr>
 *  <tr><td colspan="8">length[31:24]</td></tr>
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

#define FBP_LOGP_DATA_SIZE_MAX (FBP_LOGH_FILENAME_SIZE_MAX + FBP_LOGH_MESSAGE_SIZE_MAX)
#define FBP_LOGP_SEP '\x1f'

#ifndef FBP_LOGP_LEVEL
#define FBP_LOGP_LEVEL FBP_LOG_LEVEL_WARNING
#endif

/**
 * @brief Receive a message from the log handler
 *
 * @param user_data The api instance for this port.
 * @param header The log record header.
 * @param filename The log record filename.
 * @param message The log message.
 *
 * @return 0 or FBP_ERROR_FULL to try again later.
 * @see fbp_logh_recv
 *
 * To configure this port to forward messages from the local handler:
 * fbp_logh_dispatch_register(NULL, fbp_logp_recv, logp_api);
 */
FBP_API int32_t fbp_logp_recv(void * user_data, struct fbp_logh_header_s const * header,
                              const char * filename, const char * message);

/**
 * @brief Publish a log message received of the comm link.
 *
 * @param user_data The arbitrary user data.
 * @param header The log header, which is forwarded exactly.
 * @param filename The filename that generated this log message.
 * @param message The log message.
 * @return 0 or FBP_ERROR_NOT_AVAILABLE, FBP_ERROR_FULL.
 * @see fbp_logh_publish_formatted()
 */
typedef int32_t (*fbp_logp_publish_formatted)(void * user_data, const struct fbp_logh_header_s * header,
                                              const char * filename, const char * message);

/**
 * @brief Forward remote messages to the local handler.
 *
 * @param self This logp instance.
 * @param fn The function to call for each received comm log message,
 *      usually fbp_logh_publish_formatted.
 * @param user_data The arbitrary data for fn, usually the logh instance.
 * @see fbp_logh_publish_formatted()
 */
FBP_API void fbp_logp_handler_register(struct fbp_port_api_s * api, fbp_logp_publish_formatted fn, void * user_data);

/**
 * @brief Construct a new log port instance.
 *
 * @return The port instance for forwarding log messages.
 *
 * Populate the topic_prefix, transport, and port_id fields before
 * calling initialize.  This implementation does not use pubsub.
 *
 * If this port is expected to handle incoming log messages, call
 * fbp_logp_recv_register().
 */
FBP_API struct fbp_port_api_s * fbp_logp_factory();


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_LOG_PORT_H_ */
