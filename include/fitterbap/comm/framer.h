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
 *      <td colspan="5">frame_type[4:0]</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">length[7:0]</td></tr>
 *  <tr><td colspan="8">length_crc[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[7:0]</td></tr>
 *  <tr><td colspan="8">metadata[15:8]</td></tr>
 *  <tr><td colspan="8">... payload ...</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF</td></tr>
 * </table>
 *
 * - "SOF1" is the start of frame byte.  Although SOF1 and SOF2 are not unique
 *   and also not escaped, the SOF bytes drastically reduces the framing
 *   search space.  The SOF1 value can be selected for autobaud detection.
 *   Typical values are 0x55 and 0xAA.
 * - "SOF2" is the second start of frame byte.  The SOF2 value can be selected
 *   to ensure proper UART framing.  Typical values are 0x00.
 * - "frame_type" is the frame type identifier.  Although only 6 values are
 *   needed, they are encoded to ensure that the data frame requires 4 bit flips
 *   to become any ACK frame.  ACK frames are all separated by at least
 *   2 bit flips.
 * - "frame_id" contains an identifier that is temporally unique for all
 *   DATA frames across all ports.  The frame_id increments sequentially with
 *   each new frame and is assigned by the framer implementation.
 * - "length" is the payload length (not full frame length) in total_bytes, minus 1.
 *   The maximum payload length is 256 total_bytes.  Since the frame overhead is 9
 *   total_bytes, the actual frame length ranges from 9 to 265 total_bytes.
 * - "length_crc" is the CRC-8 computed with polynomial 0xD7 over the length field
 *   only.  This CRC has Hamming Distance (HD) of 5 over the 8-bit length.
 *   By increasing the reliability of the length field, the frame_crc
 *   remains more effective with regards to Hamming Distance.
 * - "metadata" contains arbitrary 16-bit data that is transmitted along with the
 *   message payload.  The metadata format is usually assigned by the higher-level
 *   protocol or application.  The optional @ref fbp_transport defines this field.
 *   Common "metadata" uses include:
 *   - "port" to multiplex multiple message types or endpoints onto
 *     this single byte stream, similar to a TCP port.
 *   - "start" and "stop" bits to segment and reassemble messages larger than
 *     the frame payload size.  For example,
 *     10 is start, 01 is end, 00 is middle and 11 is a single frame message.
 *   - "unique_id" that is unique for all messages in flight for a port.
 * - "payload" contains the arbitrary payload of "length" total_bytes.
 * - "frame_crc" contains the 32-bit CRC computed over the header and payload.
 *   The SOF1, SOF2, frame_crc, and EOF are excluded from the calculation.
 * - "EOF" contains an end of frame byte which allows for reliable
 *   receiver timeouts and receiver framer reset.   It also decreases the
 *   likelihood of false positive frame matches.  The value for
 *   EOF and SOF are the same, and the EOF byte is NOT consumed during
 *   framing.  Repeated SOF/EOF total_bytes between frames are ignored
 *   by the framer and can be used for autobaud detection.
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
 *      <td colspan="5">frame_type[4:0]</td>
 *      <td colspan="3">frame_id[10:8]</td>
 *  </tr>
 *  <tr><td colspan="8">frame_id[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[7:0]</td></tr>
 *  <tr><td colspan="8">frame_crc[15:8]</td></tr>
 *  <tr><td colspan="8">frame_crc[23:16]</td></tr>
 *  <tr><td colspan="8">frame_crc[31:24]</td></tr>
 *  <tr><td colspan="8">EOF </td></tr>
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
 * ## Framing algorithm
 *
 * Framing is performed by first searching for SOF1 and SOF2.  The framer
 * then validates frame_type, and candidate frames with invalid frame_types
 * are ignored.  For data frames, the framer validates the length CRC.
 *
 * The 32-bit CRC is then computed over the entire frame from the first
 * non-SOF byte through the payload, using the length byte to determine
 * the total byte count for data frames.  If the
 * frame_crc matches the computed CRC and EOF matches,
 * then the entire frame is valid.
 *
 *
 * ## Configuration options
 *
 * The fitterbap/config.h file allows for a configuration options.  The
 * following options affect the framer:
 *
 * ### 32-bit CRC:
 *
 * The 32-bit CRC uses a table-based CRC-32-CCITT by default.  As discussed
 * below, other CRC choices, such as CRC-32C (ethernet), provide even better
 * performance.  Many microcontrollers offer hardware acceleration, and you
 * can select a hardware-accelerated CRC algorithm for specific applications.
 *
 * Define FBP_CONFIG_COMM_FRAMER_CRC32(data, length) to compute the 32-bit
 * checksum.  The standard implementation uses fbp_crc32
 * declared by fitterbap/crc.h and defined by src/crc32.c.
 *
 *
 * ## CRC selection
 *
 * Cyclic redundancy check (CRC) [Wikipedia](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
 * is one approach to detecting data errors.  Unfortunately, many different,
 * incompatible CRC polynomials and variations exist.  Check out Philip Koopman's
 * [CRC Polynomial Zoo](https://users.ece.cmu.edu/~koopman/crc/crc32.html).
 *
 * CRCs guarantee that they detect a certain number of bit changes, called the
 * Hamming Distance (HD).  A CRC with an HD of 2 means that the algorithm can
 * detect all single bit changes.  It requires two bit changes before a CRC
 * could match the corrupted data.  The longer the data, the shorter the HD.
 * Some CRC polynomials are better than others.
 *
 * CRCs are also not equally effective, even with the same HD.  The
 * Hamming Weight (HW) is the number of undetectable errors given that
 * number of bits.
 *
 *
 * For our application, we compute a 32-bit CRC over 262 bytes (2096 bits).
 * So, how do common 32-bit CRCs stack up?
 *
 * [CRC-32 (ethernet)](https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb.txt):
 * HD=5 @ 2096 bits, Hamming Weight 89622
 *
 * [CRC-32C](https://users.ece.cmu.edu/~koopman/crc/c32/0x8f6e37a0.txt):
 * HD=6 @ 2096 bits, Hamming Weight 59795110
 *
 *
 * ## Analysis
 *
 * Framer performance includes the following metrics:
 * 1. False-positive frame detection rate on random data
 * 2. Frame search computational burden
 * 3. Frame locked computational burden
 *
 * To address item (2), this design uses SOF1 and SOF2 bytes that dramatically
 * reduce the search space.  This design also keeps the frame locked
 * computation burden (3) low.  The transmitter must populate the header
 * and compute the 32-bit CRC.  The receiver must validate the header fields
 * and also calculate the 32-bit CRC.  32-bit CRC is a reasonable computational
 * burden while also giving great error detection performance.
 *
 * This frame format contains multiple features to keep the false-positive
 * frame detection rate (1) low.  The features include:
 *
 * | Field        |  Accuracy    |
 * +--------------+--------------+
 * | SOF1         | 1 / 256      |
 * | SOF2         | 1 / 256      |
 * | frame_type   | 6 / 32       |
 * | frame_id     | 64 / 2^11    |
 * | length_crc   | 1 / 256      |
 * | frame_crc    | 1 / 2^32     |
 * | EOF          | 1 / 256      |
 *
 * The likelihood of a false-positive on random data is then:
 *     kb = 1/256 * 1/256 * 6/32 * 64/2**11 * 1/256 * 1/2**32 * 1/256
 *     kb = 3.2e-22
 *
 * However, we need to search the length of a frame to find the
 * start of a frame.  The maximum frame length including EOF is:
 *    fsz = sz_header + sz_payload + sz_crc32 + sz_eof
 *    fsz = 8 + 256 + 4 + 1
 *    fsz = 269 bytes = 2152 bits
 *
 * Since CRC32 provides the least protection for the longest frames,
 * the false-positive framing error rate is:
 *     k = kb * fsz
 *     k = 8.5e-20
 *
 * On average, 1 out of every 1e19 frame synchronizations will falsely
 * detect a frame.  Is this good?
 *
 * With a terrible bit error rate (BERT) of 1e-6, then the frame error rate is:
 *     be = 1e-6
 *     fe = 1 - (1 - be) ** (fsz * 8)
 *     fe = 0.00215
 *
 * At 3 Mbaud with UART N81, a frame time is:
 *     ft = fsz * 10 / 3e6
 *     ft = 0.0009
 *
 * The operation time between errors at full rate with full-sized frames is:
 *     te = ft / (fe * k) / (60 * 60 * 24 * 365)
 *     te = 154,797,983,947 years
 *
 * Which is approximately 11 times longer than the current age of the universe.
 *
 * If we have a random stream of data operating at 3 MBaud, how often will we
 * get a false positive frame?
 *
 *     fp = 1 / kb / (3e6/10) / (60 * 60 * 24 * 365)
 *     fp = 332,767,241 years
 *
 * If we have a data header match, and then completely random payload data, how
 * often do we match the end of frame indication (CRC + EOF)?
 *
 *     payload_fpe = 1/2**32 * 1/256
 *     payload_fpe = 9.1e-13
 *
 * If we drop every 1e-6 bytes running with full length frames at 3 MBaud,
 * how often will we get a false positive frame?
 *
 *     byte_error_rate = 1e-6
 *     k_bytes = 2 + 256 + 4 + 1
 *     byte_err = byte_error_rate * k_bytes
 *     fpe = byte_err * payload_fpe
 *     fpe = 2.4e-16 frames
 *     fps = (3e6/10) / (fsz - 1)
 *     t = fps / fpe / (60 * 60 * 24 * 365)
 *     t = 148,396,593,555 years
 *
 * How likely is it that the length field is interpreted incorrectly?
 *
 *     On random data, the likelihood is 1/256 (or 2**8 / 2**16 = 1/256)
 *     since each length value has one and only one corresponding CRC8 value.
 *
 * [CRC-8 0xD7](https://users.ece.cmu.edu/~koopman/crc/c08/0xeb.txt) has
 * HD=5 @ 8 bits, Hamming Weight 24
 *
 * However, assuming a 1e-6 bit error rate and the Hamming distance of 5, we
 * need 5 or more bit errors to occur to falsely match.  Using python 3 with
 * [gmpy2](https://pypi.org/project/gmpy2/)
 * [windows download](https://www.lfd.uci.edu/~gohlke/pythonlibs/#gmpy),
 * the odds of bit errors exceeding the Hamming distance are then:
 *
 *     import math
 *     import gmpy2
 *     from gmpy2 import mpfr
 *     gmpy2.get_context().precision=256
 *     # Probability of Undetected Error for any random polynomial
 *     def crc_false_positive(bit_error_rate, length_bits, hamming_distance):
 *         p = mpfr(bit_error_rate)
 *         pt = sum([math.comb(length_bits, i) * (p**i) *
 *                  (mpfr(1-p)**(length_bits-i)) for i in range(0, hamming_distance)])
 *         return 1 - pt
 *     crc_false_positive(1e-6, 8 + 8, 5)
 *     = 4e-27
 *
 * The above calculation does not account for the polynomial effectiveness
 * represented by Hamming Weight.  According to
 * [maxino09](http://users.ece.cmu.edu/~koopman/pubs/maxino09_checksums.pdf#page=3),
 * the Probability of Undetected Error accounting for CRC effectiveness is:
 *
 *     Pud = HW * BER ^ x * (1 - BER) ^ (n - x)
 *
 * According to [Koopman's CRC Zoo](https://users.ece.cmu.edu/~koopman/crc/c08/0xeb.txt),
 * the HW is 24 for this CRC over length 8.  Therefore, the likelihood at HD(5) is
 *
 *     def crc_pud(bit_error_rate, length_bits, hamming_distance, hamming_weight):
 *         ber = mpfr(bit_error_rate)
 *         return hamming_weight * (ber ** hamming_distance) * ((1 - ber) ** (length_bits - hamming_distance))
 *     crc_pud(1e-6, 8, 5, 24)
 *     = 2.4e-29
 *
 * which is a factor of 182 times better the the previous calculation.
 * Now, we can also add contributions from more bit errors:
 *
 *      crc_pud(1e-6, 8, 6, 44) + crc_pud(1e-6, 8, 7, 40) + crc_pud(1e-6, 8, 8, 45)
 *      => 4.4e-35
 *
 * but the value is insignificant compared to 2.4e-29 at HD(5).
 *
 * Now how about the CRC-32 with HD=5?
 *
 *     crc_pud(1e-6, 2096 + 32, 5, 89622)
 *     => 8.9e-26 frames
 *
 * At 3 Mbaud, we have 1119 frames / second, so our expected error rate is:
 *
 *     = 1 / 8.9e-26 / 1119 / (60 * 60 * 24 * 365)
 *     => 3.2e14 years
 *
 * Note that the alternative computation using crc_false_positive gives
 * approximately the same result:
 *
 *      = 1 / (crc_false_positive(1e-6, 2096 + 32, 5) * 2**-32) / 1119 / (60 * 60 * 24 * 365)
 *      => 3.4e14 years
 *
 * If we select CRC-32C with HD=6, the result is even better:
 *
 *     1 / crc_pud(1e-6, 2096 + 32, 6, 59795110) / 1119 / (60 * 60 * 24 * 365)
 *     => 4.7e17 years
 *
 *
 * ## References
 *
 *    - Framing & CRC
 *      - [Best CRC Polynomials - Philip Koopman](https://users.ece.cmu.edu/~koopman/crc/)
 *      - [Martin Cowen](http://blog.martincowen.me.uk/using-and-misusing-crcs.html)
 *      - [Eli Bendersky](http://eli.thegreenplace.net/2009/08/12/framing-in-serial-communications)
 *      - [StackOverflow](http://stackoverflow.com/questions/815758/simple-serial-point-to-point-communication-protocol)
 *      - [Daniel Beer](https://dlbeer.co.nz/articles/packet.html)
 *      - CRC: [Wikipedia](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
 *      - [Selection of Cyclic Redundancy Code and Checksum Algorithms to Ensure Critical Data Integrity](http://users.ece.cmu.edu/~koopman/pubs/faa15_tc-14-49.pdf)
 *    - HDLC framing
 *      - [wikipedia](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control)
 *    - Consistent Overhead Byte Stuffing (COBS):
 *      - [wikipedia](https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing)
 *      - [Embedded Related post](https://www.embeddedrelated.com/showarticle/113.php)
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
/// The framer total maximum data size in bytes, excluding EOF
#define FBP_FRAMER_MAX_SIZE (\
    FBP_FRAMER_HEADER_SIZE + \
    FBP_FRAMER_PAYLOAD_MAX_SIZE + \
    FBP_FRAMER_FOOTER_SIZE)
/// The framer link message (ACK) size in bytes, excluding EOF
#define FBP_FRAMER_LINK_SIZE (8)
#define FBP_FRAMER_OVERHEAD_SIZE (FBP_FRAMER_HEADER_SIZE + FBP_FRAMER_FOOTER_SIZE)
#define FBP_FRAMER_FRAME_ID_MAX ((1U << 11) - 1U)

/**
 * @brief The frame types.
 *
 * The 5-bit frame type values are carefully selected to ensure minimum
 * likelihood that a data frame is detected as a ACK frame.
 */
enum fbp_framer_type_e {
    FBP_FRAMER_FT_DATA = 0x00,
    FBP_FRAMER_FT_ACK_ALL = 0x0F,
    FBP_FRAMER_FT_ACK_ONE = 0x17,
    FBP_FRAMER_FT_NACK_FRAME_ID = 0x1B,
    FBP_FRAMER_FT_NACK_FRAMING_ERROR = 0x1D,  // next expect frame_id
    FBP_FRAMER_FT_RESET = 0x1E,
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
    void (*data_fn)(void *user_data, uint16_t frame_id, uint16_t metadata,
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
    uint16_t length;        // the current frame length or 0
    uint8_t buf[FBP_FRAMER_MAX_SIZE + 1];  // frame + EOF
    uint16_t buf_offset;    // the size of the buffer
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
FBP_API bool fbp_framer_validate_data(uint16_t frame_id, uint16_t metadata, uint32_t msg_size);

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
FBP_API int32_t fbp_framer_construct_data(uint8_t *b, uint16_t frame_id, uint16_t metadata,
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

/**
 * @brief Compute the CRC8 for the length field.
 *
 * @param length The length field value.
 * @return The CRC8 field.
 *
 * This function is primarily for testing.  Higher layer code does
 * not usually need this function as the framer automatically
 * populates and checks this CRC.
 */
FBP_API uint8_t fbp_framer_length_crc(uint8_t length);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_FRAMER_H_ */
