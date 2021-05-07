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
 * - 24-bits of arbitrary metadata per frame to support:
 *   - multiplexing using "ports"
 *   - Segmentation and reassembly with start & stop bits.
 *   - Message identification
 * - Full statistics, including frames transferred.
 *
 * For extremely fast transmitters (UART CDC over USB), the maximum number of
 * outstanding frames and acknowledgement turn-around time limit the total
 * rate.  USB has a typically response time of 2 milliseconds, which this
 * framer easily accommodates.  The implementation presumes that this framer
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
 * technology.  A number of decent protocols UART-style protocols
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
 * This framer contains support for backpressure by providing notifications
 * when the recipient acknowledge the frame transmission.
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
 * For memory efficiency, the protocol uses a variable message sized
 * ring buffer to store the transmit messages, which keeps bandwidth
 * fast even for small-sized messages.  The receive side uses
 * dedicated, full-frame-sized buffers.  In the common case where
 * the microcontroller to host path contains far more data than the
 * host to microcontroller path, this allows a reduced microcontroller
 * memory footprint.  Maintaining a variable length received buffer
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
 *    - Overview
 *      - [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications),
 *      - [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *      - [Daniel Beer](https://dlbeer.co.nz/articles/packet.html)
 *    - Selective Repeat Automated Repeat Request (SR-ARQ)
 *      - [wikipedia](https://en.wikipedia.org/wiki/Selective_Repeat_ARQ)
 *      - [Bandwidth-delay product @ wikipedia](https://en.wikipedia.org/wiki/Bandwidth-delay_product)
 *    - PPP
 *      - [wikipedia](https://en.wikipedia.org/wiki/Point-to-Point_Protocol),
 *      - [RFC](https://tools.ietf.org/html/rfc1661),
 *      - [Segger embOS/embNet PPP/PPPoE](https://www.segger.com/products/connectivity/emnet/add-ons/ppppppoe/)
 *    - HDLC
 *      - [wikipedia](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control)
 *    - Constant Overhead Byte Stuffing (COBS):
 *      - [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *      - [Embedded Related post](https://www.embeddedrelated.com/showarticle/113.php)
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

/// opaque data link instance.
struct fbp_dl_s;

/// The data link configuration options.
struct fbp_dl_config_s {
    uint32_t tx_link_size;    // in frames, normally the same as rx_window_size
    uint32_t tx_window_size;  // in frames
    uint32_t tx_buffer_size;  // in bytes
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

    /// The remote issued a reset command for our receive path.
    FBP_DL_EV_RX_RESET_REQUEST,

    /// The remote device is no longer responding to transmissions.
    FBP_DL_EV_TX_DISCONNECTED,

    /**
     * @brief The remote device established a transmit connection.
     *
     * We issued a reset message, and the remote responded.
     * We are now clear to transmit data to the host.  The receive
     * path is working, but we will not receive data until the
     * remote successfully completes the same process.
     */
    FBP_DL_EV_TX_CONNECTED,
};

/**
 * @brief Function called whenever a new message is sent.
 *
 * @param user_data Arbitrary user data.
 *
 * When used in a threaded environment, this function can signal the
 * thread that it should call fbp_dl_process().  This automatic
 * hook often eliminates the need for more complicated wrappers.
 */
typedef void (*fbp_dl_on_send_fn)(void * user_data);

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
 * @param metadata The arbitrary 24-bit metadata associated with the message.
 * @param msg The buffer containing the message.
 *      This buffer is only valid for the duration of the callback.
 * @param msg_size The size of msg_buffer in bytes.
 */
typedef void (*fbp_dl_recv_fn)(void *user_data, uint32_t metadata, uint8_t *msg, uint32_t msg_size);

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
 * @param metadata The arbitrary 24-bit metadata associated with the message.
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
FBP_API int32_t fbp_dl_send(struct fbp_dl_s * self, uint32_t metadata,
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
 * @brief The maximum time until the next fbp_dl_process() call.
 *
 * @param self The instance.
 * @return The maximum time in FBP time until the system must call
 *      fbp_dl_process().  The system may call process sooner.
 */
FBP_API int64_t fbp_dl_service_interval(struct fbp_dl_s * self);

/**
 * @brief Process to handle retransmission.
 *
 * @param self The instance.
 *
 * todo eliminate this function, use fbp_evm_schedule() and fbp_evm_process()
 */
FBP_API void fbp_dl_process(struct fbp_dl_s * self);

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
 * @brief Register the function called for each call to fbp_dl_send().
 *
 * @param self The data link instance.
 * @param cbk_fn The callback function.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 *
 * Threaded implementations can use this callback to set an event,
 * task notification, or file handle to tell the thread that
 * fbp_dl_process() should be invoked.
 */
FBP_API void fbp_dl_register_on_send(struct fbp_dl_s * self,
                                     fbp_dl_on_send_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Register a send-side mutex.
 *
 * @param self The data link instance.
 * @param mutex The mutex instance.
 */
FBP_API void fbp_dl_register_mutex(struct fbp_dl_s * self, fbp_os_mutex_t mutex);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_DATA_LINK_H_ */
