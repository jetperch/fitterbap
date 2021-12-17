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


#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE
#include "fitterbap/comm/framer.h"
#include "fitterbap/cdef.h"
#include "fitterbap/config.h"
#include "fitterbap/config_defaults.h"
#include "fitterbap/ec.h"
#include "fitterbap/crc.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include <inttypes.h>


#ifndef FBP_CONFIG_COMM_FRAMER_CRC32
#error Must define FBP_CONFIG_COMM_FRAMER_CRC32 in fitterbap/config.h
#endif

FBP_STATIC_ASSERT(FBP_FRAMER_LINK_SIZE == sizeof(uint64_t), link_size);

enum state_e {
    ST_SOF1,
    ST_SOF2,
    ST_FRAME_TYPE,
    ST_DATA_HEADER,
    ST_STORE,
};

struct framer_s {
    struct fbp_framer_s external_self;
    uint8_t state;          // fbp_framer_state_e
    uint8_t is_sync;
    uint16_t length;        // the current frame length or 0
    uint8_t buf[FBP_FRAMER_MAX_SIZE + 1];  // frame + EOF
    uint16_t buf_offset;    // the size of the buffer
};

/**
 * @brief The 8-bit length CRC lookup table
 *
 * Generated on Wed May 19 18:30:26 2021 by pycrc v0.9.2, https://pycrc.org
 * python -m pycrc --model=crc-8 --algorithm=table-driven --poly=0x1d7 --generate=c -o crc8.c
 *
 * using the configuration:
 *  - Width         = 8
 *  - Poly          = 0xd7
 *  - XorIn         = 0x00
 *  - ReflectIn     = False
 *  - XorOut        = 0x00
 *  - ReflectOut    = False
 *  - Algorithm     = table-driven
 */
static const uint8_t length_crc_table[256] = {
        0x00, 0xd7, 0x79, 0xae, 0xf2, 0x25, 0x8b, 0x5c, 0x33, 0xe4, 0x4a, 0x9d, 0xc1, 0x16, 0xb8, 0x6f,
        0x66, 0xb1, 0x1f, 0xc8, 0x94, 0x43, 0xed, 0x3a, 0x55, 0x82, 0x2c, 0xfb, 0xa7, 0x70, 0xde, 0x09,
        0xcc, 0x1b, 0xb5, 0x62, 0x3e, 0xe9, 0x47, 0x90, 0xff, 0x28, 0x86, 0x51, 0x0d, 0xda, 0x74, 0xa3,
        0xaa, 0x7d, 0xd3, 0x04, 0x58, 0x8f, 0x21, 0xf6, 0x99, 0x4e, 0xe0, 0x37, 0x6b, 0xbc, 0x12, 0xc5,
        0x4f, 0x98, 0x36, 0xe1, 0xbd, 0x6a, 0xc4, 0x13, 0x7c, 0xab, 0x05, 0xd2, 0x8e, 0x59, 0xf7, 0x20,
        0x29, 0xfe, 0x50, 0x87, 0xdb, 0x0c, 0xa2, 0x75, 0x1a, 0xcd, 0x63, 0xb4, 0xe8, 0x3f, 0x91, 0x46,
        0x83, 0x54, 0xfa, 0x2d, 0x71, 0xa6, 0x08, 0xdf, 0xb0, 0x67, 0xc9, 0x1e, 0x42, 0x95, 0x3b, 0xec,
        0xe5, 0x32, 0x9c, 0x4b, 0x17, 0xc0, 0x6e, 0xb9, 0xd6, 0x01, 0xaf, 0x78, 0x24, 0xf3, 0x5d, 0x8a,
        0x9e, 0x49, 0xe7, 0x30, 0x6c, 0xbb, 0x15, 0xc2, 0xad, 0x7a, 0xd4, 0x03, 0x5f, 0x88, 0x26, 0xf1,
        0xf8, 0x2f, 0x81, 0x56, 0x0a, 0xdd, 0x73, 0xa4, 0xcb, 0x1c, 0xb2, 0x65, 0x39, 0xee, 0x40, 0x97,
        0x52, 0x85, 0x2b, 0xfc, 0xa0, 0x77, 0xd9, 0x0e, 0x61, 0xb6, 0x18, 0xcf, 0x93, 0x44, 0xea, 0x3d,
        0x34, 0xe3, 0x4d, 0x9a, 0xc6, 0x11, 0xbf, 0x68, 0x07, 0xd0, 0x7e, 0xa9, 0xf5, 0x22, 0x8c, 0x5b,
        0xd1, 0x06, 0xa8, 0x7f, 0x23, 0xf4, 0x5a, 0x8d, 0xe2, 0x35, 0x9b, 0x4c, 0x10, 0xc7, 0x69, 0xbe,
        0xb7, 0x60, 0xce, 0x19, 0x45, 0x92, 0x3c, 0xeb, 0x84, 0x53, 0xfd, 0x2a, 0x76, 0xa1, 0x0f, 0xd8,
        0x1d, 0xca, 0x64, 0xb3, 0xef, 0x38, 0x96, 0x41, 0x2e, 0xf9, 0x57, 0x80, 0xdc, 0x0b, 0xa5, 0x72,
        0x7b, 0xac, 0x02, 0xd5, 0x89, 0x5e, 0xf0, 0x27, 0x48, 0x9f, 0x31, 0xe6, 0xba, 0x6d, 0xc3, 0x14
};

static inline uint8_t length_crc(uint8_t length) {
    return length_crc_table[length];
}

static inline uint8_t parse_frame_type(uint8_t const * frame) {
    return (frame[2] >> 3) & 0x1f;
}

static inline uint16_t parse_frame_id(uint8_t const * frame) {
    return (((uint16_t) (frame[2] & 0x7)) << 8) | frame[3];
}

static inline uint8_t parse_data_payload_raw_length(uint8_t const * frame) {
    return frame[4];
}

static inline uint8_t parse_data_length_crc(uint8_t const * frame) {
    return frame[5];
}

static inline uint16_t parse_data_payload_length(uint8_t const * frame) {
    return 1 + ((uint16_t) parse_data_payload_raw_length(frame));
}

static inline uint16_t parse_data_metadata(uint8_t const * frame) {
    return frame[6] | (((uint16_t) frame[7]) << 8);
}

static void handle_framing_error(struct framer_s * self, uint16_t discard_size) {
    self->state = ST_SOF1;
    self->external_self.status.ignored_bytes += discard_size;
    self->buf_offset = 0;
    if (self->is_sync) {
        ++self->external_self.status.resync;
        self->is_sync = 0;
        if (self->external_self.api.framing_error_fn) {
            self->external_self.api.framing_error_fn(self->external_self.api.user_data);
        }
    }
}

static bool handle_frame(struct framer_s * self, const uint8_t * p) {
    uint8_t frame_type = parse_frame_type(p);
    uint16_t frame_id = parse_frame_id(p);
    uint16_t frame_sz = self->length - 1;
    bool rv = true;

    if (p[frame_sz] != FBP_FRAMER_SOF1) {  // check EOF (SOF of next frame)
        FBP_LOGD1("missing EOF");
        handle_framing_error(self, self->length);
        return false;  // state = SOF1, not sync
    } else if (frame_type == FBP_FRAMER_FT_DATA) {
        // Compute frame CRC (data length CRC was already validated)
        uint8_t const * crc_value = p + frame_sz - FBP_FRAMER_FOOTER_SIZE;
        uint32_t crc_rx = ((uint32_t) crc_value[0])
                          | (((uint32_t) crc_value[1]) << 8)
                          | (((uint32_t) crc_value[2]) << 16)
                          | (((uint32_t) crc_value[3]) << 24);
        uint32_t crc_calc = FBP_CONFIG_COMM_FRAMER_CRC32(p + 2, frame_sz - FBP_FRAMER_FOOTER_SIZE - 2);
        if (crc_rx != crc_calc) {
            FBP_LOGD1("crc invalid");
            handle_framing_error(self, frame_sz);
            rv = false;    // state = SOF2, still sync
        } else {
            uint16_t metadata = parse_data_metadata(p);
            uint16_t payload_length = parse_data_payload_length(p);
            FBP_LOGD3("data frame received, frame_id=%d, metadata=0x%04" PRIx32 ", %d bytes",
                      (int) frame_id, (uint32_t) metadata, (int) payload_length);
            if (self->external_self.api.data_fn) {
                self->external_self.api.data_fn(self->external_self.api.user_data, frame_id, metadata,
                                  (uint8_t *) (p + FBP_FRAMER_HEADER_SIZE), payload_length);
            }
        }
    } else {
        FBP_LOGD3("link frame received, frame_type=0x%02x, frame_id=%d", frame_type, frame_id);
        if ((p[0] != (uint8_t) ~p[4]) || (p[1] != (uint8_t) ~p[5]) ||
            (p[2] != (uint8_t) ~p[6]) || (p[3] != (uint8_t) ~p[7])) {
            FBP_LOGD1("link check invalid");
            handle_framing_error(self, frame_sz);
            rv = false;    // state = SOF2, still sync
        } else if (self->external_self.api.link_fn) {
            self->external_self.api.link_fn(self->external_self.api.user_data, frame_type, frame_id);
        }
    }
    self->is_sync = 1;
    self->state = ST_SOF2;
    self->buf[0] = FBP_FRAMER_SOF1;
    self->buf_offset = 1;
    return rv;
}

static void ll_recv(struct fbp_framer_s * ext_self, uint8_t const * buffer, uint32_t buffer_size) {
    struct framer_s * self = (struct framer_s *) ext_self;
    FBP_LOGD3("received %d bytes", (int) buffer_size);
    self->external_self.status.total_bytes += buffer_size;
    const uint8_t * nocopy = NULL;

    while (buffer_size) {
        self->buf[self->buf_offset++] = *buffer++;
        --buffer_size;

        switch (self->state) {
            case ST_SOF1:
                if (self->buf[0] == FBP_FRAMER_SOF1) {
                    self->state = ST_SOF2;
                } else if (self->is_sync) {  // expected SOF, but missed
                    FBP_LOGD1("Expected SOF1 got 0x%02x", self->buf[0]);
                    handle_framing_error(self, 1);
                } else {  // continue search
                    // optimized for speed, skip handle_framing_error() call
                    self->external_self.status.ignored_bytes += 1;
                    self->buf_offset = 0;
                }
                break;

            case ST_SOF2:
                if (self->buf[1] == FBP_FRAMER_SOF2) {
                    FBP_LOGD3("SOF");
                    self->state = ST_FRAME_TYPE;
                } else if (self->buf[1] == FBP_FRAMER_SOF1) {
                    // allow duplicate SOF1 bytes
                    self->buf_offset = 1;
                    ++self->external_self.status.ignored_bytes;
                } else {
                    FBP_LOGD1("Expected SOF2 got 0x%02x", self->buf[1]);
                    handle_framing_error(self, 2);
                }
                break;

            case ST_FRAME_TYPE:
                nocopy = buffer - 3;
                switch (parse_frame_type(self->buf)) {
                    case FBP_FRAMER_FT_DATA:
                        self->length = 0;
                        self->state = ST_DATA_HEADER;
                        break;
                    case FBP_FRAMER_FT_ACK_ALL:            /* Intentional fall-through */
                    case FBP_FRAMER_FT_ACK_ONE:            /* Intentional fall-through */
                    case FBP_FRAMER_FT_NACK_FRAME_ID:      /* Intentional fall-through */
                    case FBP_FRAMER_FT_NACK_FRAMING_ERROR: /* Intentional fall-through */
                    case FBP_FRAMER_FT_RESET:
                        self->state = ST_STORE;
                        self->length = FBP_FRAMER_LINK_SIZE + 1;
                        break;
                    default:
                        handle_framing_error(self, 3);
                        break;
                }
                break;

            case ST_DATA_HEADER:
                if (self->buf_offset >= (FBP_FRAMER_HEADER_SIZE - 2)) {
                    uint8_t crc1 = length_crc_table[parse_data_payload_raw_length(self->buf)];
                    uint8_t crc2 = parse_data_length_crc(self->buf);
                    if (crc1 == crc2) {
                        self->state = ST_STORE;
                        uint16_t payload_length = parse_data_payload_length(self->buf);
                        self->length = payload_length + FBP_FRAMER_OVERHEAD_SIZE + 1;
                        if (nocopy && ((nocopy + self->length) <= (buffer + buffer_size))) {
                            uint32_t consume_len = 2 + payload_length + FBP_FRAMER_FOOTER_SIZE + 1;
                            handle_frame(self, nocopy);
                            buffer += consume_len;
                            buffer_size -= consume_len;
                        }
                    } else {
                        handle_framing_error(self, 6);
                        break;
                    }
                }
                break;

            case ST_STORE:
                if ((buffer_size) && (self->buf_offset < self->length)) {
                    uint32_t remaining = self->length - self->buf_offset;
                    uint32_t sz = remaining;
                    if (buffer_size < remaining) {
                        sz = buffer_size;
                    }
                    memcpy(self->buf + self->buf_offset, buffer, sz);
                    self->buf_offset += sz;
                    buffer += sz;
                    buffer_size -= sz;
                }
                if (self->buf_offset >= self->length) {
                    handle_frame(self, self->buf);
                }
                break;
        }
    }
}

static void reset(struct fbp_framer_s * ext_self) {
    struct framer_s * self = (struct framer_s *) ext_self;
    self->state = ST_SOF1;
    self->is_sync = 0;
    self->length = 0;
    self->buf_offset = 0;
    fbp_memset(&self->external_self.status, 0, sizeof(self->external_self.status));
}

static int32_t construct_data(
        struct fbp_framer_s * ext_self,
        uint8_t * b, uint16_t frame_id, uint16_t metadata,
        uint8_t const *msg, uint32_t msg_size) {
    (void) ext_self;
    if ((msg_size < 1) || (msg_size > 256) || (frame_id > FBP_FRAMER_FRAME_ID_MAX)) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    uint8_t length_field = msg_size - 1;
    b[0] = FBP_FRAMER_SOF1;
    b[1] = FBP_FRAMER_SOF2;
    b[2] = (FBP_FRAMER_FT_DATA << 3) | ((frame_id >> 8) & 0x7);
    b[3] = (uint8_t) (frame_id & 0xff);
    b[4] = length_field;
    b[5] = length_crc_table[length_field];
    b[6] = metadata & 0xff;
    b[7] = (metadata >> 8) & 0xff;
    memcpy(b + FBP_FRAMER_HEADER_SIZE, msg, msg_size);
    uint32_t crc = FBP_CONFIG_COMM_FRAMER_CRC32(b + 2, msg_size + FBP_FRAMER_HEADER_SIZE - 2);
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 0] = crc & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 1] = (crc >> 8) & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 2] = (crc >> 16) & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 3] = (crc >> 24) & 0xff;
    return 0;
}

static int32_t construct_link(
        struct fbp_framer_s * ext_self,
        uint64_t * b, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    (void) ext_self;
    if ((frame_type & 0x1f) != frame_type) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if (frame_id > FBP_FRAMER_FRAME_ID_MAX) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    switch (frame_type) {
        case FBP_FRAMER_FT_ACK_ALL:                /* intentional fall-through */
        case FBP_FRAMER_FT_ACK_ONE:                /* intentional fall-through */
        case FBP_FRAMER_FT_NACK_FRAME_ID:          /* intentional fall-through */
        case FBP_FRAMER_FT_NACK_FRAMING_ERROR:     /* intentional fall-through */
        case FBP_FRAMER_FT_RESET:
            break;
        default:
            return FBP_ERROR_PARAMETER_INVALID;
    }
    uint32_t h = FBP_FRAMER_SOF1
            | (((uint32_t) FBP_FRAMER_SOF2) << 8)
            | (((uint32_t) (frame_type << 3) | ((frame_id >> 8) & 0x7)) << 16)
            | (((uint32_t) (frame_id & 0xff)) << 24);
    *b = h | (((uint64_t) ~h) << 32);
    return 0;
}

uint8_t fbp_framer_length_crc(uint8_t length) {
    return length_crc(length);
}

static void finalize(struct fbp_framer_s * self) {
    fbp_free(self);
}

/**
 * @brief Allocate and initialize a new framer instance.
 *
 * @return The new data link instance.
 */
FBP_API struct fbp_framer_s * fbp_framer_initialize() {
    struct framer_s * self = fbp_alloc_clr(sizeof(struct framer_s));
    struct fbp_framer_s * x = &self->external_self;
    x->recv = ll_recv;
    x->reset = reset;
    x->construct_data = construct_data;
    x->construct_link = construct_link;
    x->finalize = finalize;
    reset(&self->external_self);
    return x;
}
