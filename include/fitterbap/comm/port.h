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
 * @brief Port API.
 */

#ifndef FBP_COMM_PORT_API_H_
#define FBP_COMM_PORT_API_H_

#include <stdint.h>
#include <stdbool.h>
#include "fitterbap/cmacro_inc.h"
#include "fitterbap/comm/transport.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/pubsub.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_port Transport Port API
 *
 * @brief Transport port API.
 *
 * @{
 */

FBP_CPP_GUARD_START

// forward declaration
struct fbp_port_api_s;

/**
 * @brief The port API, used by the transport layer to interact
 *      with (potentially) dynamically instantiated ports.
 *
 * This structure defines a "class" in C with virtual function
 * callbacks, initialize, finalize, on_event, and on_recv.
 * The port factory function used to construct this instance normally
 * populates the callback fields and the meta field.
 *
 * The caller of the factory function, usually the communication stack, then
 * populates the other fields: pubsub, topic_prefix, transport, port_id,
 * and evm before calling initialize().
 *
 * The factory instance will often define its own internal
 * structure.  This API struct can be the first field, which
 * allows the callbacks to recast API instances to the
 * internal structure type.  Alternatively, it can use FBP_CONTAINER_OF
 * if the API structure is not the first field.
 */
struct fbp_port_api_s {
    /**
     * @brief The JSON metadata structure defining the port.
     */
    const char * meta;

    /**
     * @brief The function called to initialize the port instance.
     *
     * @param self The port instance.
     * @return 0 or error code.
     */
    int32_t (*initialize)(struct fbp_port_api_s * self);

    /**
     * @brief The function called to finalized the port instance.
     *
     * @param self The port instance.
     * @return 0 or error code.
     */
    int32_t (*finalize)(struct fbp_port_api_s * self);

    /**
     * @brief The function called on transport events.
     */
    fbp_transport_event_fn on_event;

    /**
     * @brief The function called when transport receives data for this port.
     */
    fbp_transport_recv_fn on_recv;

    /**
     * @brief The PubSub instance.
     *
     * Ports must use PubSub to communicate with the rest of the system for
     * command and control.  Ports may optionally provide a direct
     * interface for streaming data for higher performance.
     */
    struct fbp_pubsub_s * pubsub;

    /**
     * @brief The topic prefix for PubSub messages.
     *
     * The topic prefix, empty or ending with '/', that this
     * port should use for all topics that it publishes to pubsub.
     */
    const char * topic_prefix;

    /**
     * @brief The transport instance for sending data.
     */
    struct fbp_transport_s * transport;

    /**
     * @brief The port's port_id for sending data.
     */
    uint8_t port_id;

    /**
     * @brief The event manager for this port.
     *
     * Ports use the event manager to schedule callback events.
     */
    struct fbp_evm_api_s * evm;
};

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_PORT_API_H_ */
