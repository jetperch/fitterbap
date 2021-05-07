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
 * @brief Message framer and multiplexer for byte streams.
 */

#ifndef FBP_COMM_FRAMER_H_
#define FBP_COMM_FRAMER_H_

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_framer Framer for byte streams
 *
 * @brief Provide reliable byte stream framing with robust error detection.
 *
 * This framer protocol provides two different frame formats:
 * - data frame
 * - link frame used by acks, nacks, and reset
 *
 * The data frame format is variable length:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF1[7:0]</td></tr>
 *  <tr><td colspan="8">SOF2[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type[2:0]</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[15:8]</td></tr>
 *  <tr><td colspan="8">metadata[23:16]</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF (optional)</td></tr>
 * </table>
 *
 * - "SOF1" is the start of frame byte.  Although SOF1 and SOF2 are not unique
 *   and also not escaped, the SOF bytes drastically reduces the framing
 *   search space.  The SOF1 value can be selected for autobaud detection.
 *   Typical values are 0x55 and 0xAA.
 * - "SOF2" is the second start of frame byte.  The SOF2 value can be selected
 *   to ensure proper UART framing.  Typical values are 0x00.
 * - "frame_type" is the frame type identifier.
 *   - 000: data
 *   - 100: acknowledge (ACK)
 *   - 110: not acknowledge (NACK)
 *   - all other values are invalid and must be discarded.
 * - "frame_id" contains an identifier that is temporally unique for all
 *   DATA frames across all ports.  The frame_id increments sequentially with
 *   each new frame and is assigned by the framer implementation.
 * - "length" is the payload length (not full frame length) in total_bytes, minus 1.
 *   The maximum payload length is 256 total_bytes.  Since the frame overhead is 9
 *   total_bytes, the actual frame length ranges from 9 to 265 total_bytes.
 * - "metadata" contains arbitrary 24-bit data that is transmitted along with the
 *   message payload.  The metadata format is usually assigned by the higher-level
 *   protocol or application.  The optional @ref fbp_transport defines this field.
 *   Common "metadata" uses include:
 *   - "port" to multiplex multiple message types or endpoints onto
 *     this single byte stream, similar to a TCP port.
 *   - "start" and "stop" bits to segment and reassemble messages larger than
 *     the frame payload size.  For example,
 *     10 is start, 01 is end, 00 is middle and 11 is a single frame message.
 *   - "unique_id" that is unique for all messages in flight for a prot.
 * - "payload" contains the arbitrary payload of "length" total_bytes.
 * - "frame_crc" contains the CRC32 computed over the header and payload.
 *   The SOF, frame_crc and EOF total_bytes are excluded from the calculation.
 * - "EOF" contains an optional end of frame byte which allows for reliable
 *   receiver timeouts and receiver framer reset.   The value for
 *   EOF and SOF are the same.  Repeated SOF/EOF total_bytes between frames are ignored
 *   by the framer and can be used for autobaud detection.
 *
 * Framing is performed by first searching for the sync byte.  The CRC-32-CCITT
 * is computed over the entire frame from the first non-SOF byte through
 * payload, using the length byte to determine the total byte count.  If the
 * frame_crc total_bytes match the computed CRC, then the entire frame is valid.
 *
 * The link frame format is a fixed-length frame with
 * 8 total_bytes:
 *
 * <table class="doxtable message">
 *  <tr><th>7</td><th>6</td><th>5</td><th>4</td>
 *      <th>3</td><th>2</td><th>1</td><th>0</td></tr>
 *  <tr><td colspan="8">SOF1[7:0]</td></tr>
 *  <tr><td colspan="8">SOF2[7:0]</td></tr>
 *  <tr>
 *      <td colspan="3">frame_type</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="1">rsv=0</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF (optional)</td></tr>
 * </table>
 *
 * The link frame frame_types are used by the data link layer
 * to manage retransmissions.  The types are:
 *
 * - ACK_ALL: Receiver has all frame_ids up to and including the
 *   indicated frame_id.
 * - ACK_ONE: Receiver has received frame_id, but is missing
 *   one or more previous frame_ids.
 * - NACK_FRAME_ID: Receiver did not correctly receive the frame_id.
 * - NACK_FRAMING_ERROR: A framing error occurred.  The frame_id
 *   indicates the most recent, correctly received frame.
 *   Note that this may not be lowest frame_id.
 * - RESET: Reset all state.
 *
 *
 * ## Analysis
 *
 * Framer performance includes the following metrics:
 * 1. False-positive frame detection rate
 * 2. Frame search computational burden
 * 3. Frame locked computational burden
 *
 * To address item (2), this design uses SOF1 and SOF2 bytes that dramatically
 * reduce the search space.  This design also keeps the frame locked
 * computation burden (2) low.  The transmitter must populate the header
 * and compute the CRC32.  The receiver must validate the header fields
 * and also calculate the CRC32.  CRC32 is a reasonable computational
 * burden while also giving great error detection performance.
 *
 * This frame format contains multiple features to keep the false-positive
 * frame detection rate (1) low.  The features include:
 *
 * | Field        |  Accuracy    |
 * +--------------+--------------+
 * | SOF1         | 1 / 256      |
 * | SOF2         | 1 / 256      |
 * | frame_type   | 3 / 8        |
 * | rsv          | 1 / 4        |
 * | frame_id     | 64 / 2^11    |
 * | frame_crc    | 1 / 2^32     |
 *
 * The likelihood of a false-positive on random data is then:
 *     kb = 1/256 * 1/256 * 3/8 * 1/4 * 64/2**11 * 1/2**32
 *     kb = 1e-17
 *
 * However, we need to search the length of a frame to find the
 * start of a frame.  The maximum frame length is:
 *    fsz = sz_header + sz_payload + sz_crc32
 *    fsz = 8 + 256 + 4
 *    fsz = 265
 *
 * Since CRC32 provides the least protection for the longest frames,
 * the false-positive framing error rate is:
 *     k = kb * fsz
 *     k = 2.8e-15
 *
 * On average, 1 out of every 3e14 frame synchronizations will falsely
 * detect a frame.  Is this good?
 *
 * With a terrible bit error rate (BERT) of 1e-6, then the frame error rate is:
 *     be = 1e-6
 *     fe = 1 - (1 - be) ** (fsz * 8)
 *     fe = 0.00214
 *
 * At 3 Mbaud with UART N81, a frame time is:
 *     ft = fsz * 10 / 3e6
 *     ft = 0.0009
 *
 * The operation time between errors at full rate with full-sized frames is:
 *     te = ft / (fe * k) / (60 * 60 * 24 * 365)
 *     te = 4,741,667 years
 *
 * With an operational life of 10 years, we expect that 0.00021% of units will
 * experience this error.  Even if this error completely bricks the device,
 * this error rate is acceptable.
 *
 * The existing implementation does not check for EOF/SOF at the end of the
 * frame.  If we were to modify the transmitter to send EOF when idle, then
 * this would further decrease the error rate by 1/256.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The value for the first start of frame byte.
#define FBP_FRAMER_SOF1 ((uint8_t) 0x55)
/// The value for the second start of frame byte.
#define FBP_FRAMER_SOF2 ((uint8_t) 0x00)
/// The framer header size in total_bytes.
#define FBP_FRAMER_HEADER_SIZE (8)
/// The maximum payload size in total_bytes.
#define FBP_FRAMER_PAYLOAD_MAX_SIZE (256)
/// The framer footer size in total_bytes.
#define FBP_FRAMER_FOOTER_SIZE (4)
/// The framer total maximum size in total_bytes
#define FBP_FRAMER_MAX_SIZE (\
    FBP_FRAMER_HEADER_SIZE + \
    FBP_FRAMER_PAYLOAD_MAX_SIZE + \
    FBP_FRAMER_FOOTER_SIZE)
/// The maximum available number of ports
#define FBP_FRAMER_LINK_SIZE (8)
#define FBP_FRAMER_OVERHEAD_SIZE (FBP_FRAMER_HEADER_SIZE + FBP_FRAMER_FOOTER_SIZE)
#define FBP_FRAMER_FRAME_ID_MAX ((1 << 11) - 1)
#define FBP_FRAMER_MESSAGE_ID_MAX ((1 << 24) - 1)

/// The frame types.
enum fbp_framer_type_e {
    FBP_FRAMER_FT_DATA = 0x0,
    FBP_FRAMER_FT_ACK_ALL = 0x1,
    FBP_FRAMER_FT_INVALID1 = 0x2,
    FBP_FRAMER_FT_ACK_ONE = 0x3,
    FBP_FRAMER_FT_NACK_FRAME_ID = 0x4,
    FBP_FRAMER_FT_INVALID2 = 0x5,
    FBP_FRAMER_FT_NACK_FRAMING_ERROR = 0x6,  // next expect frame_id
    FBP_FRAMER_FT_RESET = 0x7,
};

/// The framer status.
struct fbp_framer_status_s {
    uint64_t total_bytes;
    uint64_t ignored_bytes;
    uint64_t resync;
};

/**
 * @brief The API event callbacks to the upper layer.
 */
struct fbp_framer_api_s {
    /// The arbitrary user data.
    void *user_data;

    /**
     * @brief The function to call on data frames.
     *
     * @param user_data The arbitrary user data.
     * @param frame_id The frame id.
     * @param metadata The metadata.
     * @param msg The message buffer.
     * @param msg_size The size of msg_buffer in bytes.
     */
    void (*data_fn)(void *user_data, uint16_t frame_id, uint32_t metadata,
                    uint8_t *msg, uint32_t msg_size);

    /**
     * @brief The function to call on link frames.
     *
     * @param user_data The arbitrary user data.
     * @param frame_type The frame type.
     * @param frame_id The frame id.
     */
    void (*link_fn)(void *user_data, enum fbp_framer_type_e frame_type, uint16_t frame_id);

    /**
     * @brief The function to call on any framing errors.
     *
     * @param user_data The arbitrary user data.
     */
    void (*framing_error_fn)(void *user_data);
};

/// The framer instance.
struct fbp_framer_s {
    struct fbp_framer_api_s api;
    uint8_t state;    // fbp_framer_state_e
    uint8_t is_sync;
    uint16_t length;
    uint8_t buf[FBP_FRAMER_MAX_SIZE];
    uint16_t buf_offset;
    struct fbp_framer_status_s status;
};

/**
 * @brief Provide receive data to the framer.
 *
 * @param self The framer instance.
 * @param buffer The data received, which is only valid for the
 *      duration of the callback.
 * @param buffer_size The size of buffer in total_bytes.
 */
FBP_API void fbp_framer_ll_recv(struct fbp_framer_s *self,
                                uint8_t const *buffer, uint32_t buffer_size);

/**
 * @brief Reset the framer state.
 *
 * @param self The framer instance.
 *
 * The caller must initialize the ul parameter correctly.
 */
FBP_API void fbp_framer_reset(struct fbp_framer_s *self);

/**
 * @brief Validate the fbp_framer_construct_data() parameters.
 *
 * @param frame_id The frame id for the frame.
 * @param metadata The message metadata.
 * @param msg_size The size of msg_buffer in bytes.
 * @return True if parameters are valid, otherwise false.
 */
FBP_API bool fbp_framer_validate_data(uint16_t frame_id, uint32_t metadata, uint32_t msg_size);

/**
 * @brief Construct a data frame.
 *
 * @param b The output buffer, which must be at least msg_size + FBP_FRAMER_OVERHEAD_SIZE bytes.
 * @param frame_id The frame id for the frame.
 * @param metadata The message metadata
 * @param msg The payload buffer.
 * @param msg_size The size of msg_buffer in bytes.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_framer_construct_data(uint8_t *b, uint16_t frame_id, uint32_t metadata,
                                          uint8_t const *msg, uint32_t msg_size);

/**
 * @brief Validate the fbp_framer_construct_link() parameters.
 *
 * @param frame_type The link frame type.
 * @param frame_id The frame id.
 * @return True if parameters are valid, otherwise false.
 */
FBP_API bool fbp_framer_validate_link(enum fbp_framer_type_e frame_type, uint16_t frame_id);

/**
 * @brief Construct a link frame.
 *
 * @param b The output buffer, which must be at least FBP_FRAMER_LINK_SIZE bytes.
 * @param frame_type The link frame type.
 * @param frame_id The frame id.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_framer_construct_link(uint8_t *b, enum fbp_framer_type_e frame_type, uint16_t frame_id);

/**
 * @brief Compute the difference between frame ids.
 *
 * @param a The first frame id.
 * @param b The second frame_id.
 * @return The frame id difference of a - b.
 */
FBP_API int32_t fbp_framer_frame_id_subtract(uint16_t a, uint16_t b);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_FRAMER_H_ */
