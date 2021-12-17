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
 * @brief Stream stack.
 */

#ifndef FBP_COMM_STACK_H_
#define FBP_COMM_STACK_H_

#include "fitterbap/common_header.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/comm/transport.h"
#include "fitterbap/comm/port0.h"
#include "fitterbap/comm/log_port.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/comm/pubsub_port.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_stack Full stream stack
 *
 * @brief Full stream stack.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The stack object.
 *
 * The stack is intentionally transparent so that applications
 * can reach the pieces parts if needed.  However, use direct
 * access with care as it makes your code more brittle.
 */
struct fbp_stack_s {
    struct fbp_framer_s * framer;
    struct fbp_dl_s * dl;
    struct fbp_evm_api_s evm_api;
    struct fbp_transport_s * transport;
    struct fbp_port0_s * port0;
    struct fbp_pubsub_s * pubsub;
    struct fbp_pubsubp_s * pubsub_port;
    struct fbp_port_api_s * logp;
};

/**
 * Initialize the communication stack.
 *
 * @param config The data-link layer configuration.
 * @param port0_mode The communication link mode: host or client.
 * @param topic_prefix The prefix for this stack.  Each port will be
 *      use a {topic_prefix}/{port_id} prefix.
 * @param ll_instance The lower-level communication implementation.
 * @param evm_api The event manager API.
 * @param pubsub The pubsub instance for this device.
 * @param timesync The timesync instance, for clients that want to
 *      be responsible for synchronizing time.  Otherwise, NULL.
 * @return The stack instance or NULL on error.
 */
struct fbp_stack_s * fbp_stack_initialize(
        struct fbp_dl_config_s const * config,
        enum fbp_port0_mode_e port0_mode,
        const char * topic_prefix,
        struct fbp_evm_api_s * evm_api,
        struct fbp_dl_ll_s const * ll_instance,
        struct fbp_pubsub_s * pubsub,
        struct fbp_ts_s * timesync
);

/**
 * Finalize the communication stack and free all resources.
 *
 * @param self The stack instance.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_stack_finalize(struct fbp_stack_s * self);

/**
 * @brief Set the mutex used by the stack.
 *
 * @param self The stack instance.
 * @param mutex The mutex to use.  Provide NULL to clear.
 */
FBP_API void fbp_stack_mutex_set(struct fbp_stack_s * self, fbp_os_mutex_t mutex);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_STACK_H_ */
