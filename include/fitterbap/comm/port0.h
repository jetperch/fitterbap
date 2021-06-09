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
 * @brief Port 0 definitions.
 */

#ifndef FBP_COMM_PORT0_H_
#define FBP_COMM_PORT0_H_

#include <stdint.h>
#include <stdbool.h>
#include "fitterbap/cmacro_inc.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/comm/transport.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_port0 Transport Port 0
 *
 * @brief Transport port 0.
 *
 * Port 0 allocates port_data:
 *      port_data[7]: 0=request or unused, 1=response
 *      port_data[6:3]: reserved, 0
 *      port_data[2:0]: The fbp_port0_op_e operation.
 * @{
 */

FBP_CPP_GUARD_START

/// The default transmit timeout, in milliseconds.
#define FBP_PORT0_TIMEOUT_MS (250)

/// The character offset for the port_id to ensure printable ASCII text.
#define FBP_PORT0_META_CHAR_OFFSET (32)

/**
 * @brief The service operations provided by port 0.
 */
enum fbp_port0_op_e {
    FBP_PORT0_OP_UNKNOWN = 0,

    /**
     * @brief Share link statistics.
     *
     * One side requests, and the other side provides its statistics.
     *
     * req_payload=ignore
     * rsp_payload=fbp_dl_status_s
     */
    FBP_PORT0_OP_STATUS = 1,

    /**
     * @brief Echo payloads.
     *
     * If we receive a request, respond with the same payload.
     * The payload length and contents are arbitrary.
     */
    FBP_PORT0_OP_ECHO = 2,

    /**
     * @brief Synchronize clocks.
     *
     * The payload contains 5 x 64-bit values:
     * 0: reserved for future use, set to 0
     * 1: source transmit time counter u64, populated by req, repeated by rsp
     * 2: target receive time i64, ignored in req, populated by rsp
     * 3: target transmit time i64, ignored in req, populated by rsp
     * 4: reserved for source received time u64, set to 0
     *
     * In the case that the server does not yet know the UTC time,
     * it should reply with 0.
     */
    FBP_PORT0_OP_TIMESYNC = 3,

    /**
     * @brief Retrieve port metadata definitions.
     *
     * On request, the payload is ignored.
     * On response, the payload contains:
     * - one byte containing (32 + port_id)
     * - a NULL-terminated JSON formatted string.
     *
     * The JSON response structure consists of:
     * - name: A user-meaningful "name" key.
     * - type: The port type, which is one of
     *   oam, pubsub, stream, msg, sample
     *
     * Types define other keys:
     * - oam: Operations, administration and management.  This port 0 only.
     * - pubsub: The publish-subscribe port.  Not other keys defined.
     * - text: Provides UTF-8 text communication, often for a command console,
     *   such as SCPI.  The "protocol" key describes the actual protocol.
     * - stream: An data stream with custom format.
     * - msg: Raw messages with custom payload format.
     * - wave_src: Produce binary data samples.  Each message on this
     *   port contains a 32-bit sample identifier corresponding to the first
     *   sample in the message followed by packed sample data.
     *   The additional metadata keys are:
     *   - prefix: The PubSub topic prefix for controlling this stream.
     *     See below for the required subtopics.
     *   The sample PubSub subtopics are:
     *   - ctrl: bool on/off for the binary sample stream
     *   - fs: The maximum available frequency for this stream (ro)
     *   - div: The optional available sampling frequency divisor(s)
     *   - format: The optional list of supported data formats.  Formats include:
     *     - Single bit is b.  This format is packed with bit N in
     *       byte 0, bit 0.  Bit N+1 goes to byte 0, bit 1.
     *       Bit N+8 goes to byte 1, bit 0.
     *     - IEEE floating point is either f32 or f64.
     *     - Signed integers are iZ where Z is a multiple of 4.
     *     - Unsigned integer are uZ where Z is a multiple of 4.
     *     - Signed fixed-point integers are iMqN where M+N is a multiple of 4.
     *     - Unsigned fixed-point integers are uMqN where M+N is a multiple of 4.
     *   - compress: The optional list of available compression algorithms.
     *
     *   The integer types are fully packed in little-endian format.  For types
     *   with odd nibbles. The even samples are represented "normally", and the odd
     *   samples fill their most significant nibble in the upper 4 bits of the same
     *   byte occupied by the even sample's most significant nibble.
     *
     * - wave_sink: Receive binary data samples from a wave_src at
     *   the other end of the connection.
     *
     * If the port is not defined, respond with an empty string "" consisting
     * of only the NULL terminator.
     */
    FBP_PORT0_OP_META = 4,

    /**
     * @brief Negotiate the data link properties.
     *
     * The data link initially starts with the transmit window set to 1,
     * which is the same as STOP & WAIT.  Both sides send their maximum
     * receive message queue size using this message.  The other side
     * then adjusts their transmit message queue to the minimum of
     * the received queue size and their local transmit queue size.
     *
     * The payload consists of 32-bit values:
     * - version: major8.minor8.patch16
     * - sender's maximum receive window size
     */
    FBP_PORT0_OP_NEGOTIATE = 5,

    /**
     * @brief Enter raw UART loopback mode for bit error rate testing.
     *
     * @todo Not yet supported.
     */
    FBP_PORT0_OP_RAW = 6,
};

enum fbp_port0_mode_e {
    FBP_PORT0_MODE_CLIENT, ///< Clients sync time.
    FBP_PORT0_MODE_SERVER, ///< Servers provide reference time.
};

/// Opaque port0 instance.
struct fbp_port0_s;

// The opaque PubSub instance, from "pubsub.h"
struct fbp_pubsub_s;

// The opaque timesync instance from "timesync.h"
struct fbp_ts_s;

extern const char FBP_PORT0_META[];

/**
 * @brief Allocate and initialize the instance.
 *
 * @param mode The port0 operating mode for this instance.
 * @param dl The data link instance.
 * @param evm The event manager for this instance.
 * @param transport The transport instance.
 * @param send_fn The function to call to send data, which should be
 *      fbp_transport_send() except during unit testing.
 * @param pubsub The pubsub instance for event updates.
 * @param topic_prefix The prefix to use for pubsub.
 * @param timesync The timesync instance, for clients that want to
 *      be responsible for synchronizing time.  Otherwise, NULL.
 * @return The new instance or NULL on error.
 */
FBP_API struct fbp_port0_s * fbp_port0_initialize(enum fbp_port0_mode_e mode,
        struct fbp_dl_s * dl,
        struct fbp_evm_api_s * evm,
        struct fbp_transport_s * transport,
        fbp_transport_send_fn send_fn,
        struct fbp_pubsub_s * pubsub,
        const char * topic_prefix,
        struct fbp_ts_s * timesync);

/**
 * @brief Finalize and deallocate the instance.
 *
 * @param self The port0 instance.
 */
FBP_API void fbp_port0_finalize(struct fbp_port0_s * self);

/**
 * @brief The function to call when the transport layer receives an event.
 *
 * @param self The instance.
 * @param event The event.
 *
 * This function can be safely cast to fbp_transport_event_fn and provided
 * to fbp_transport_port_register().
 *
 */
FBP_API void fbp_port0_on_event_cbk(struct fbp_port0_s * self, enum fbp_dl_event_e event);

/**
 * @brief The function to call when the transport layer receives a message.
 *
 * @param self The instance.
 * @param port_id The port identifier (should be 0).
 * @param seq The frame reassembly information.
 * @param port_data The port-defined metadata.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 *
 * This function can be safely cast to fbp_dl_recv_fn and provided
 * to fbp_transport_port_register().
 */
FBP_API void fbp_port0_on_recv_cbk(struct fbp_port0_s * self,
                                   uint8_t port_id,
                                   enum fbp_transport_seq_e seq,
                                   uint8_t port_data,
                                   uint8_t *msg, uint32_t msg_size);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_PORT0_H_ */
