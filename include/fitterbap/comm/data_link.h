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
 * @brief Reliable data link layer for byte streams.
 */

#ifndef FBP_COMM_DATA_LINK_H_
#define FBP_COMM_DATA_LINK_H_

#include "fitterbap/cmacro_inc.h"
#include "fitterbap/comm/framer.h"
#include "fitterbap/event_manager.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/version.h"
#include <stdint.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_data_link Data link layer for byte streams
 *
 * @brief Provide reliable framing and retransmission over byte
 *      streams, such as UARTs.
 *
 * This module provides a reliable data link layer for message transmission
 * over byte streams.  The data link uses
 * Selective Repeat Automated Repeat Request (SR-ARQ).  The protocol
 * features rapid no acknowledgements (NACK) which helps minimize the
 * RAM buffering requirements for the protocol.
 *
 * The framing is provided by the @ref fbp_framer.
 *
 * The features of this data link include:
 *
 * - Robust framing using SOF bytes, length, and CRC32.
 * - Fast recovery on errors for minimal RAM usage.
 * - Reliable transmission with acknowledgements and not acknowledgements.
 * - Guaranteed in-order delivery.
 * - Multiple pending transmit frames for maximum throughput.
 * - 16-bits of arbitrary metadata per frame to support:
 *   - multiplexing using "ports"
 *   - Segmentation and reassembly with start & stop bits.
 *   - Message identification
 * - Full statistics, including frames transferred.
 *
 * For extremely fast transmitters (UART CDC over USB), the maximum number of
 * outstanding frames and acknowledgement turn-around time limit the total
 * rate.  USB has a typically response time of 2 milliseconds, which this
 * implementation easily accommodates.  The implementation presumes that it
 * is running in the same thread as the receive message processing and
 * can keep up with the raw data rate.
 *
 * The protocol consists of two different frame formats:
 * - data frame
 * - link frame used by acks, nacks, and reset
 *
 * See @ref fbp_framer for the frame details.
 *
 *
 * ## Why another protocol?
 *
 * As described below, this data link layer implements 1990's
 * technology.  A number of decent UART-style protocols
 * exist, but performance is not great.  When designs need
 * robust performance, TCP/IP is the proven solution.  Unfortunately,
 * TCP/IP through a stack like lwIP requires significant code
 * space and RAM.  Also performance is not great.  This library
 * easily and efficiently handles 3 Mbaud connections.
 *
 *
 * ## Retranmissions
 *
 * This data link layer provides reliable message delivery with
 * error detection and retransmissions.  The implemention uses
 * Selective Repeat Automated Repeat Request (SR-ARQ), similar to
 * what TCP of TCP/IP uses.
 *
 * The transmitter constructs and sends frames with an incrementing frame_id
 * that rolls over to zero after reaching the maximum.  When the receiver
 * receives the next frame in the sequence, it sends an ACK_ALL back to the
 * sender.  When the sender receives an ACK_ALL, it can recycle the frame
 * buffers for all frame_ids up to the ACK_ALL.
 *
 * If the receiver sees a skip in frame_id values, it immediately sends
 * a NACK for the missing frame along with an ACK_ONE for the valid frame.
 * When the receive finally receives the missing frame_id, it will respond
 * with ACK_ALL with the frame_id for the group.
 *
 * The transmitter can have at most frame_id_max / 2 frames - 1 outstanding,
 * which we call frame_pend_max.  The receiver treats
 * frame_id - frame_pend_max (computed with wrap) as the "past" and
 * frame_id + frame_pend_max (computed with wrap) as the "future".
 *
 * When the receiver receives a "past" frame, the receiver discards the frame
 * but does send an ACK to the transmitter.
 * If the receiver receives a "future" frame beyond the expected next frame_id,
 * the the receiver must have missed at least one frame.  If the skipped
 * frame was previously idle, then the receiver immediately sends a
 * NACK_FRAME_ID for the skipped frame and marks it as NACKed.
 * If it is skipped again, the
 * receiver does not generate a NACK_FRAME_ID.  Retranmission then relies
 * upon the transmitter timeout.
 *
 * If the received frame is beyond the receive window, then the receiver
 * response with NACK.  Otherwise, it response with ACK_ONE and stores
 * the frame to a buffer.  The receiver will continue to store other
 * future frames and reply with ACK_ONE.  When it finally receives the
 * skipped frame, it replies with ACK_ALL with the most recent frame_id
 * corresponding with no skips.
 *
 * If the receiver receives a framing error,
 * it immediately sends a NACK_FRAMING_ERROR containing the frame_id
 * of the expected frame.
 *
 * When the transmitter receives a NACK, it normally complete the current
 * frame transmission.  The transmitter should then
 * retransmit starting with the frame_id indicated in the NACK.  The
 * transmitter knows the outstanding frames.
 *
 * The transmitter also contains a timeout.  If the transmitter does not
 * receive an ACK without the timeout, it will retransmit the frame.
 * The transmitter will retransmit indefinitely, but will indicate an
 * error after a threshold.
 *
 * This protocol contains support for backpressure by providing notifications
 * when the recipient acknowledges the frame transmission.
 * The application can configure the desired number of pending
 * send frames based upon memory availability and application complexity.
 *
 *
 * ## Bandwidth analysis
 *
 * The protocol provides support for up to 2 ^ 11 / 2  - 1 = 1023 frames in flight.
 * With maximum payload size of 256 total_bytes and 10 total_bytes overhead on a 3 Mbaud link,
 * the protocol is capable of a maximum outstanding duration of:
 *
 *     (1023 * (256 + 10)) / (3000000 / 10) = 0.907 seconds
 *
 * However, the transmitter must have a transmit buffer of at least 272,118 total_bytes,
 * too much for many intended microcontrollers.  A typical microcontroller may
 * allocate 32 outstanding frames, which is 8512 total_bytes.  To prevent stalling the
 * link with full payloads, the transmitter receive must ACK within:
 *
 *     (32 * (256 + 10)) / (3000000 / 10) = 28 milliseconds
 *
 * For memory management simplicity, the implementation uses two fixed-size
 * message buffers, one for transmit and one for receive.  A previous
 * implementation used a variable-sized transmit message buffer, but the
 * memory savings were shown to be insignificant at the cost of more
 * complexity and processing.  Maintaining a variable length received buffer
 * is much more complicated due to out of order reception and
 * message retirement.
 *
 * The required RAM buffer is a function of the
 * [bandwidth-delay product](https://en.wikipedia.org/wiki/Bandwidth-delay_product).
 * You typically want at least twice the bandwidth-delay product of buffering
 * with SR-ARQ to keep maximum data rates.  Less demanding applications
 * can reduce the RAM buffer.
 *
 * Direct communication between microcontrollers with fast interrupt handling
 * and low delays can use small buffers.  Communication to a host computer
 * requires larger buffers due to the host computer delay variability.
 * Under Windows, you may want to ensure that your timer resolution is set to
 * 1 millisecond or less.  You can use
 * [TimerTool](https://github.com/tebjan/TimerTool) to check.
 *
 *
 * ## References
 *
 *    - Selective Repeat Automated Repeat Request (SR-ARQ)
 *      - [wikipedia](https://en.wikipedia.org/wiki/Selective_Repeat_ARQ)
 *      - [Bandwidth-delay product @ wikipedia](https://en.wikipedia.org/wiki/Bandwidth-delay_product)
 *    - PPP
 *      - [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      - [RFC](https://tools.ietf.org/html/rfc1661),
 *      - [Segger embOS/embNet PPP/PPPoE](https://www.segger.com/products/connectivity/emnet/add-ons/ppppppoe/)
 *    - Alternatives
 *      - [Microcontroller Interconnect Network](https://github.com/min-protocol/min)
 *      - [Telemetry](https://github.com/Overdrivr/Telemetry)
 *      - [SerialFiller](https://github.com/gbmhunter/SerialFiller)
 *      - [TinyFrame](https://github.com/MightyPork/TinyFrame)
 *      - [P2P UART Network](https://github.com/bowen-liu/P2P-UART-Network)
 *
 * @{
 */

FBP_CPP_GUARD_START


#define FBP_DL_VERSION_MAJOR 1
#define FBP_DL_VERSION_MINOR 0
#define FBP_DL_VERSION_PATCH 0
#define FBP_DL_VERSION  FBP_VERSION_ENCODE_U32(FBP_DL_VERSION_MAJOR, FBP_DL_VERSION_MINOR, FBP_DL_VERSION_PATCH)


/// opaque data link instance.
struct fbp_dl_s;

/// The data link configuration options.
struct fbp_dl_config_s {
    uint32_t tx_link_size;    // in frames, normally the same as rx_window_size
    uint32_t tx_window_size;  // in frames
    uint32_t rx_window_size;  // in frames
    int64_t tx_timeout;       // transmit timeout in FBP time 34Q30.
};

/// The data link transmit status.
struct fbp_dl_tx_status_s {
    uint64_t bytes;
    uint64_t msg_bytes;
    uint64_t data_frames;
    uint64_t retransmissions;
};

/// The data link receive status.
struct fbp_dl_rx_status_s {
    uint64_t msg_bytes;
    uint64_t data_frames;
};

/**
 * @brief The data link instance statistics.
 */
struct fbp_dl_status_s {
    uint32_t version;
    uint32_t reserved;
    struct fbp_dl_rx_status_s rx;
    struct fbp_framer_status_s rx_framer;
    struct fbp_dl_tx_status_s tx;
};

/**
 * @brief The events signalled by the data link layer to the next higher layer.
 */
enum fbp_dl_event_e {
    /// An unknown event occurred (should never happen).
    FBP_DL_EV_UNKNOWN,

    /// The remote issued a reset command.
    FBP_DL_EV_RESET_REQUEST,

    /// The remote device is no longer responding to transmissions.
    FBP_DL_EV_DISCONNECTED,

    /**
     * @brief The remote device established a transmit connection.
     *
     * We received a reset message and cleared our RX & TX state.
     */
    FBP_DL_EV_CONNECTED,

    /**
     * @brief The connection is established.
     *
     * This event is injected using fbp_dl_event_inject().
     *
     * In the reference implementation, all ports except for port0
     * should wait for this event before communicating.
     * port0 negotiates parameters and exchanges port information
     * before injecting this event.  This provides a controlled
     * mechanism to bring up the system.
     */
    FBP_DL_EV_TRANSPORT_CONNECTED,

    /**
     * @brief The application is connected.
     *
     * This event is injected using fbp_dl_event_inject().
     *
     * In the reference implementation, all ports except for port0
     * and the PubSub port should wait for this event before communicating.
     * After port0 emits FBP_DL_EV_TRANSPORT_CONNECTED, the PubSub port
     * negotiates and exchanges data.  Upon completion, the PubSub port
     * emits this event.
     */
    FBP_DL_EV_APP_CONNECTED,
};

/**
 * @brief The function called on events.
 *
 * @param user_data The arbitrary user data.
 * @param event The signaled event.
 */
typedef void (*fbp_dl_event_fn)(void *user_data, enum fbp_dl_event_e event);

/**
 * @brief The function called upon message receipt.
 *
 * @param user_data The arbitrary user data.
 * @param metadata The arbitrary 16-bit metadata associated with the message.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 */
typedef void (*fbp_dl_recv_fn)(void *user_data, uint16_t metadata, uint8_t *msg, uint32_t msg_size);

/**
 * @brief The API event callbacks to the upper layer.
 */
struct fbp_dl_api_s {
    void *user_data;            ///< The arbitrary user data.
    fbp_dl_event_fn event_fn;  ///< Function called on events.
    fbp_dl_recv_fn recv_fn;    ///< Function call on received messages.
};

/**
 * @brief Send a message.
 *
 * @param self The instance.
 * @param metadata The arbitrary 16-bit metadata associated with the message.
 * @param msg The msg_buffer containing the message.  The driver
 *      copies this buffer, so it only needs to be valid for the duration
 *      of the function call.
 * @param msg_size The size of msg_buffer in total_bytes.
 * @param timeout_ms The timeout duration in milliseconds.  Values <= 0 do not
 *      retry and fail immediately if the buffer is full.  Values > 0 will
 *      retry until success or until the timeout_ms elapses.
 * @return 0 or error code.
 *
 * The port send_done_cbk callback will be called when the send completes.
 */
FBP_API int32_t fbp_dl_send(struct fbp_dl_s * self, uint16_t metadata,
                            uint8_t const *msg, uint32_t msg_size,
                            uint32_t timeout_ms);

/**
 * @brief Provide receive data to this data link instance.
 *
 * @param self The data link instance.
 * @param buffer The data received, which is only valid for the
 *      duration of the callback.
 * @param buffer_size The size of buffer in total_bytes.
 */
FBP_API void fbp_dl_ll_recv(struct fbp_dl_s * self,
                            uint8_t const * buffer, uint32_t buffer_size);

/**
 * @brief Write data to the low-level driver instance.
 *
 * @param user_data The instance.
 * @param buffer The buffer containing the data to send. The caller retains
 *      ownership, and the buffer is only valid for the duration of the call.
 * @param buffer_size The size of buffer in total_bytes.
 */
typedef void (*fbp_dl_ll_send_fn)(void * user_data, uint8_t const * buffer, uint32_t buffer_size);

/**
 * @brief The number of bytes currently available to send().
 *
 * @param user_data The instance.
 * @return The non-blocking free space available to send().
 */
typedef uint32_t (*fbp_dl_ll_send_available_fn)(void * user_data);

/**
 * @brief The low-level abstract driver implementation.
 */
struct fbp_dl_ll_s {
    /**
     * @brief The low-level driver instance.
     *
     * This value is passed as the first variable to each
     * low-level driver callback.
     */
    void * user_data;

    /// Function to send data.
    fbp_dl_ll_send_fn send;

    /// Function to get the number of bytes currently available to send().
    fbp_dl_ll_send_available_fn send_available;

    // note: recv is performed through the fbp_dl_ll_recv with the dl instance.
};

/**
 * @brief Allocate, initialize, and start the data link layer.
 *
 * @param config The data link configuration.
 * @param evm The event manager instance.
 * @param ll_instance The lower-level driver instance.
 * @return The new data link instance.
 */
FBP_API struct fbp_dl_s * fbp_dl_initialize(
        struct fbp_dl_config_s const * config,
        struct fbp_evm_api_s const * evm,
        struct fbp_dl_ll_s const * ll_instance);

/**
 * @brief Register callbacks for the upper-layer.
 *
 * @param self The data link instance.
 * @param ul The upper layer
 */
FBP_API void fbp_dl_register_upper_layer(struct fbp_dl_s * self, struct fbp_dl_api_s const * ul);

/**
 * @brief Reset the data link state.
 *
 * @param self The data link instance.
 *
 * WARNING: "client" devices call this function on FBP_DL_EV_RECEIVED_RESET
 * to reset their transmission path.  Normally, the transmitter will
 * automatically signal a link-layer reset to the receiver upon connection
 * lost.  Call this function with caution.
 */
FBP_API void fbp_dl_reset_tx_from_event(struct fbp_dl_s * self);

/**
 * @brief Stop, finalize, and deallocate the data link instance.
 *
 * @param self The data link instance.
 * @return 0 or error code.
 *
 * While this method is provided for completeness, most embedded systems will
 * not use it.  Implementations without "free" may fail.
 */
FBP_API int32_t fbp_dl_finalize(struct fbp_dl_s * self);

/**
 * @brief Get the status for the data link.
 *
 * @param self The data link instance.
 * @param status The status instance to populate.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_dl_status_get(
        struct fbp_dl_s * self,
        struct fbp_dl_status_s * status);

/**
 * @brief Clear the status.
 *
 * @param self The data link instance.
 */
FBP_API void fbp_dl_status_clear(struct fbp_dl_s * self);

/**
 * @brief Register a send-side mutex.
 *
 * @param self The data link instance.
 * @param mutex The mutex instance.
 */
FBP_API void fbp_dl_register_mutex(struct fbp_dl_s * self, fbp_os_mutex_t mutex);

/**
 * @brief Get the maximum allowed TX window size.
 *
 * @param self The data link instance.
 * @return The maximum TX window size provided to fbp_dl_initialize().
 */
FBP_API uint32_t fbp_dl_tx_window_max_get(struct fbp_dl_s * self);


/**
 * @brief Set the effective TX window size.
 *
 * @param self The data link instance.
 * @param tx_window_size The maximum window size.
 *
 * When the data link layer starts, it always starts with the effective
 * TX window set to 1.  This allows a higher-level MAC protocol to
 * negotiate the effective window size.  The transmit window size can
 * be increased up to the maximum provided to fbp_dl_initialize().
 * After being set, the window size cannot change until reset.
 */
FBP_API void fbp_dl_tx_window_set(struct fbp_dl_s * self, uint32_t tx_window_size);

/**
 * @brief Get the configured RX window size.
 *
 * @param self The data link instance.
 * @return The RX window size.
 *
 * This method is provided for the higher-level MAC layer to negotiate
 * the window size.  No other modules should use this function.
 */
FBP_API uint32_t fbp_dl_rx_window_get(struct fbp_dl_s * self);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_DATA_LINK_H_ */
