/*
 * Copyright 2020-2021 Jetperch LLC
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
 * @brief UART data link.
 */

#ifndef FBP_HOST_COMM_H_
#define FBP_HOST_COMM_H_

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/comm/log_port.h"
#include <stdint.h>

/**
 * @ingroup fbp_host
 * @defgroup fbp_host_comm Host comm stack
 *
 * @brief Host communications stack.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The opaque communications object.
struct fbp_comm_s;

/**
 * @brief Initialize a new host communications interface.
 *
 * @param config The data link configuration.
 * @param device The device name.
 * @param baudrate The baud rate, for UART devices.
 * @param cbk_fn The function to call on topic updates.  This function
 *      must remain valid until fbp_comm_finalize().
 * @param cbk_user_data The arbitrary data for cbk_fn().
 * @return The new instance or NULL.
 */
FBP_API struct fbp_comm_s * fbp_comm_initialize(
        struct fbp_dl_config_s const * config,
        const char * device,
        uint32_t baudrate,
        fbp_pubsub_subscribe_fn cbk_fn,
        void * cbk_user_data);

/**
 * @brief Finalize the comm instance and free all resources.
 *
 * @param self The communications instance.
 */
FBP_API void fbp_comm_finalize(struct fbp_comm_s * self);

/**
 * @brief Publish to a topic.
 *
 * @param self The communications instance.
 * @param topic The topic to update.
 * @param value The new value for the topic.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_comm_publish(
    struct fbp_comm_s * self,
    const char * topic,
    const struct fbp_union_s * value);

/**
 * @brief Get the retained value for a topic.
 *
 * @param self The communications instance.
 * @param topic The topic name.
 * @param[out] value The current value for topic.  Since this request is
 *      handled in the caller's thread, it does not account
 *      for any updates queued.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_comm_query(
        struct fbp_comm_s * self,
        const char * topic,
        struct fbp_union_s * value);

/**
 * @brief Get the status for the data link.
 *
 * @param self The communications instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_comm_status_get(
        struct fbp_comm_s * self,
        struct fbp_dl_status_s * status);

/**
 * @brief Register a log message receive callback.
 *
 * @param self The communications instance.
 * @param cbk_fn The function to call on received messages.
 *      Provide NULL to unregister.
 * @param cbk_user_data The arbitrary user data for cbk_fn.
 */
FBP_API void fbp_comm_log_recv_register(struct fbp_comm_s * self, fbp_logp_on_recv cbk_fn, void * cbk_user_data);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_HOST_COMM_H_ */
