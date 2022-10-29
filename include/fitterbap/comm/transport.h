/*
 * Copyright 2014-2021 Jetperch LLC
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
 * @brief Transport layer for byte streams.
 */

#ifndef FBP_COMM_TRANSPORT_H_
#define FBP_COMM_TRANSPORT_H_

#include "fitterbap/common_header.h"
#include "fitterbap/comm/data_link.h"
#include <stdint.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_transport Transport layer for byte streams
 *
 * @brief Provide port multiplexing and segmentation / reassembly.
 *
 * This transport layer assigns the data-link layer metadata field as:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr>
 *      <td colspan="1">start</td>
 *      <td colspan="1">stop</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="5">port_id[4:0]</td>
 *  </tr>
 *  <tr><td colspan="8">user_data[7:0]</td></tr>
 * </table>
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The maximum allowed port number.
#define FBP_TRANSPORT_PORT_MAX (0x1FU)

enum fbp_transport_seq_e {
    FBP_TRANSPORT_SEQ_MIDDLE = 0,
    FBP_TRANSPORT_SEQ_STOP = 1,
    FBP_TRANSPORT_SEQ_START = 2,
    FBP_TRANSPORT_SEQ_SINGLE = 3,
};

/// Opaque transport instance.
struct fbp_transport_s;

/**
 * @brief The function called on events.
 *
 * @param user_data The arbitrary user data.
 * @param event The signaled event.
 */
typedef void (*fbp_transport_event_fn)(void *user_data, enum fbp_dl_event_e event);

/**
 * @brief The function called upon message receipt.
 *
 * @param user_data The arbitrary user data.
 * @param port_id The port id for this port.
 * @param seq The frame reassembly information.
 * @param port_data The arbitrary 8-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 */
typedef void (*fbp_transport_recv_fn)(void *user_data,
                                      uint8_t port_id,
                                      enum fbp_transport_seq_e seq,
                                      uint8_t port_data,
                                      uint8_t *msg, uint32_t msg_size);

/**
 * @brief The function called to send a message to the data link layer.
 *
 * @param user_data The arbitrary user data (data link layer instance).
 * @param metadata The arbitrary 16-bit metadata associated with the message.
 * @param msgr The msg_buffer containing the message.  The driver
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 */
typedef int32_t (*fbp_transport_ll_send)(void * user_data, uint16_t metadata,
                                         uint8_t const *msg, uint32_t msg_size);

/**
 * @brief The function type used by upper layers to send a message.
 *
 * @param self The instance.
 * @param port_id The port id for this port.
 * @param seq The frame reassembly information.
 * @param port_data The arbitrary 8-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The msg_buffer containing the message.  The data link layer
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @param timeout_ms The timeout duration in milliseconds.  Values <= 0 do not
 *      retry and fail immediately if the buffer is full.  Values > 0 will
 *      retry until success or until the timeout_ms elapses.
 * @return 0 or error code.
 */
typedef int32_t (*fbp_transport_send_fn)(struct fbp_transport_s * self,
                                         uint8_t port_id,
                                         enum fbp_transport_seq_e seq,
                                         uint8_t port_data,
                                         uint8_t const *msg, uint32_t msg_size);

/**
 * @brief Allocate and initialize the instance.
 *
 * @param send_fn The function called to send data.  Normally, provide
 *      (fbp_transport_ll_send) fbp_dl_send.
 * @param send_user_data The arbitrary data for send_fn.
 * @return 0 or error code.
 */
FBP_API struct fbp_transport_s * fbp_transport_initialize(fbp_transport_ll_send send_fn, void * send_user_data);

/**
 * @brief Finalize and deallocate the instance.
 *
 * @param self The transport instance.
 */
FBP_API void fbp_transport_finalize(struct fbp_transport_s * self);

/**
 * @brief Register (or deregister) port callbacks.
 *
 * @param self The transport instance.
 * @param port_id The port_id to register.
 * @param meta The JSON metadata string describing this port function.
 *      This caller retains ownership, but this string must remain valid
 *      until fbp_transport_finalize().
 *      See the function details for the format.
 * @param event_fn The function to call on events, which may be NULL.
 * @param recv_fn The function to call on data received, which may be NULL.
 * @param user_data The arbitrary data for event_fn and recv_fn.
 * @return 0 or error code.
 *
 * The event_fn will be called from within this function to update
 * the current transmit connection status.
 *
 * The meta JSON string describes the functions of this port.
 * Clients MUST provide this string.  Servers may provide NULL if
 * the port is fully defined by the client.  The JSON string is
 * an object and must contain a "type" key.  The well-known port
 * types are:
 *
 * - oam: The operations, administration, & management port (port 0).
 *   See "port0.h".
 * - pubsub: The pubsub port (port 1).  See "pubsub_port.h".
 * - terminal: UTF-8 text terminal.
 * - waveform: Binary sample data.  See "waveform_port.h".
 */
FBP_API int32_t fbp_transport_port_register(struct fbp_transport_s * self,
                                            uint8_t port_id,
                                            const char * meta,
                                            fbp_transport_event_fn event_fn,
                                            fbp_transport_recv_fn recv_fn,
                                            void * user_data);

/**
 * @brief Register (or deregister) the default port callbacks.
 *
 * @param self The transport instance.
 * @param event_fn The function to call on events, which may be NULL.
 * @param recv_fn The function to call on data received for
 *      ports with recv_fn NULL, which may be NULL.
 * @param user_data The arbitrary data for event_fn and recv_fn.
 * @return 0 or error code.
 *
 * The event_fn will be called from within this function to update
 * the current transmit connection status.
 */
FBP_API int32_t fbp_transport_port_register_default(
        struct fbp_transport_s * self,
        fbp_transport_event_fn event_fn,
        fbp_transport_recv_fn recv_fn,
        void * user_data);

/**
 * @brief Send a message.
 *
 * @param self The instance.
 * @param port_id The port id for this port.
 * @param seq The frame reassembly information.
 * @param port_data The arbitrary 8-bit port data.  Each port is
 *      free to assign meaning to this value.
 * @param msg The msg_buffer containing the message.  The data link layer
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_transport_send(struct fbp_transport_s * self,
                                   uint8_t port_id,
                                   enum fbp_transport_seq_e seq,
                                   uint8_t port_data,
                                   uint8_t const *msg, uint32_t msg_size);

/**
 * @brief The function to call when the lower layer receives an event.
 *
 * @param self The instance.
 * @param event The event.
 *
 * This function can be safely cast to fbp_dl_event_fn and provided
 * to fbp_dl_register_upper_layer().
 *
 */
FBP_API void fbp_transport_on_event_cbk(struct fbp_transport_s * self, enum fbp_dl_event_e event);

/**
 * @brief The function to call when the lower-layer receives a message.
 *
 * @param self The instance.
 * @param metadata The arbitrary 16-bit metadata associated with the message.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 *
 * This function can be safely cast to fbp_dl_recv_fn and provided
 * to fbp_dl_register_upper_layer().
 */
FBP_API void fbp_transport_on_recv_cbk(struct fbp_transport_s * self, uint16_t metadata,
                                       uint8_t *msg, uint32_t msg_size);

/**
 * @brief Get the port metadata.
 *
 * @param self The instance.
 * @param port_id The port_id for the metadata.
 * @return The metadata, or NULL if not present.
 */
FBP_API const char * fbp_transport_meta_get(struct fbp_transport_s * self, uint8_t port_id);

/**
 * @brief Inject the TRANSPORT or APP CONNECTED events.
 *
 * @param self The transport instance.
 * @param event The allowed event to inject.  Only FBP_DL_EV_TRANSPORT_CONNECTED
 *      and FBP_DL_EV_APP_CONNECTED are currently allowed.
 *      All other events not allowed.
 *
 * Use care with this function.  Normally,
 * port0 injects FBP_DL_EV_TRANSPORT_CONNECTED and
 * pubsubp injects FBP_DL_EV_APP_CONNECTED.
 */
FBP_API void fbp_transport_event_inject(struct fbp_transport_s * self, enum fbp_dl_event_e event);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_TRANSPORT_H_ */
