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


#define FBP_LOG_LEVEL FBP_LOG_LEVEL_INFO
#include "fitterbap/comm/framer.h"
#include "fitterbap/ec.h"
#include "fitterbap/crc.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"


enum state_e {
    ST_SOF1,
    ST_SOF2,
    ST_FRAME_TYPE,
    ST_DATA_LENGTH,
    ST_STORE,
};


struct recv_buf_s {
    uint8_t const * buf;
    uint16_t size;
};

static void recv(struct fbp_framer_s * self, struct recv_buf_s * buf);

static inline uint8_t parse_frame_type(uint8_t const * frame) {
    return (frame[2] >> 5) & 0x7;;
}

static inline uint16_t parse_data_frame_id(uint8_t const * frame) {
    return (((uint16_t) (frame[2] & 0x7)) << 8) | frame[4];
}

static inline uint16_t parse_link_frame_id(uint8_t const * frame) {
    return (((uint16_t) (frame[2] & 0x7)) << 8) | frame[3];
}

static inline uint16_t parse_data_payload_length(uint8_t const * frame) {
    return 1 + ((uint16_t) frame[3]);
}

static inline uint32_t parse_data_metadata(uint8_t const * frame) {
    return ((uint32_t) frame[5])
        | (((uint32_t) frame[6]) << 8)
        | (((uint32_t) frame[7]) << 16);
}

static bool validate_crc(uint8_t const * frame) {
    uint16_t frame_sz;
    if (parse_frame_type(frame) == FBP_FRAMER_FT_DATA) {
        frame_sz = parse_data_payload_length(frame) + FBP_FRAMER_OVERHEAD_SIZE;
    } else {
        frame_sz = FBP_FRAMER_LINK_SIZE;
    }
    uint8_t const * crc_value = frame + frame_sz - FBP_FRAMER_FOOTER_SIZE;
    uint32_t crc_rx = ((uint32_t) crc_value[0])
        | (((uint32_t) crc_value[1]) << 8)
        | (((uint32_t) crc_value[2]) << 16)
        | (((uint32_t) crc_value[3]) << 24);
    uint32_t crc_calc = fbp_crc32(0, frame + 2, frame_sz - FBP_FRAMER_FOOTER_SIZE - 2);
    return (crc_rx == crc_calc);
}

static void handle_framing_error(struct fbp_framer_s * self) {
    self->state = ST_SOF1;
    if (self->is_sync) {
        ++self->status.resync;
        self->is_sync = 0;
        if (self->api.framing_error_fn) {
            self->api.framing_error_fn(self->api.user_data);
        }
    }
}

static void handle_framing_error_discard(struct fbp_framer_s * self) {
    handle_framing_error(self);
    self->status.ignored_bytes += self->buf_offset;
    self->buf_offset = 0;
    self->state = ST_SOF1;
    self->length = 0;
}

static inline uint8_t recv_buf_advance(struct recv_buf_s * buf) {
    uint8_t u8 = buf->buf[0];
    ++buf->buf;
    --buf->size;
    return u8;
}

static void reprocess_buffer(struct fbp_framer_s * self) {
    handle_framing_error(self);

    self->state = ST_SOF1;
    self->length = 0;
    struct recv_buf_s buf = {
            .buf = self->buf + 1,
            .size = self->buf_offset - 1
    };
    self->status.ignored_bytes += 1;
    self->buf_offset = 0;
    recv(self, &buf);
}

static void handle_frame(struct fbp_framer_s * self) {
    uint8_t frame_type = parse_frame_type(self->buf);
    if (self->buf_offset != self->length) {
        FBP_LOGW("consume frame length error: %d != %d",
                  (int) self->buf_offset, (int) self->length);
        reprocess_buffer(self);
    } else {
        if (frame_type == FBP_FRAMER_FT_DATA) {
            if (self->api.data_fn) {
                uint16_t frame_id = parse_data_frame_id(self->buf);
                uint32_t metadata = parse_data_metadata(self->buf);
                uint16_t payload_length = parse_data_payload_length(self->buf);
                self->api.data_fn(self->api.user_data, frame_id, metadata,
                                  self->buf + FBP_FRAMER_HEADER_SIZE, payload_length);
            }
        } else {
            if (self->api.link_fn) {
                self->api.link_fn(self->api.user_data, frame_type,
                                  parse_link_frame_id(self->buf));
            }
        }
        self->state = ST_SOF1;
        self->buf_offset = 0;
        self->length = 0;
    }
}

static void recv(struct fbp_framer_s * self, struct recv_buf_s * buf) {
    while (buf->size) {
        self->buf[self->buf_offset++] = recv_buf_advance(buf);

        switch (self->state) {
            case ST_SOF1:
                self->length = 0;
                if (self->buf[0] == FBP_FRAMER_SOF1) {
                    self->state = ST_SOF2;
                } else {
                    if (self->is_sync) {
                        FBP_LOGW("Expected SOF1 got 0x%02x", self->buf[0]);
                    }
                    handle_framing_error_discard(self);
                }
                break;

            case ST_SOF2:
                self->length = 0;
                if (self->buf[1] == FBP_FRAMER_SOF2) {
                    FBP_LOGD3("SOF");
                    self->state = ST_FRAME_TYPE;
                } else if (self->buf[1] == FBP_FRAMER_SOF1) {
                    // allow duplicate SOF1 bytes
                    self->buf_offset = 1;
                    ++self->status.ignored_bytes;
                } else {
                    FBP_LOGW("Expected SOF2 got 0x%02x", self->buf[1]);
                    handle_framing_error_discard(self);
                }
                break;

            case ST_FRAME_TYPE:
                if (self->buf[2] & 0x18) {
                    FBP_LOGW("Invalid reserved bits: 0x%02x", self->buf[2]);
                    handle_framing_error_discard(self);
                    break;
                }
                switch (parse_frame_type(self->buf)) {
                    case FBP_FRAMER_FT_DATA:
                        self->state = ST_DATA_LENGTH;
                        break;
                    case FBP_FRAMER_FT_ACK_ALL:            /* Intentional fall-through */
                    case FBP_FRAMER_FT_ACK_ONE:            /* Intentional fall-through */
                    case FBP_FRAMER_FT_NACK_FRAME_ID:      /* Intentional fall-through */
                    case FBP_FRAMER_FT_NACK_FRAMING_ERROR: /* Intentional fall-through */
                    case FBP_FRAMER_FT_RESET:
                        self->state = ST_STORE;
                        self->length = FBP_FRAMER_LINK_SIZE;
                        break;
                    default:
                        handle_framing_error_discard(self);
                        break;
                }
                break;

            case ST_DATA_LENGTH:
                self->state = ST_STORE;
                self->length = parse_data_payload_length(self->buf) + FBP_FRAMER_OVERHEAD_SIZE;
                break;

            case ST_STORE:
                if ((buf->size) && (self->buf_offset < self->length)) {
                    uint32_t remaining = self->length - self->buf_offset;
                    uint32_t sz = remaining;
                    if (buf->size < remaining) {
                        sz = buf->size;
                    }
                    memcpy(self->buf + self->buf_offset, buf->buf, sz);
                    self->buf_offset += sz;
                    buf->buf += sz;
                    buf->size -= sz;
                }
                if (self->buf_offset >= self->length) {
                    if (!validate_crc(self->buf)) {
                        FBP_LOGI("crc invalid");
                        ++self->status.resync;
                        reprocess_buffer(self);
                        break;
                    } else {
                        FBP_LOGD3("frame received, %d bytes", (int) self->length);
                        self->is_sync = true;
                        handle_frame(self);
                    }
                }
                break;
        }
    }
}

void fbp_framer_ll_recv(struct fbp_framer_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    FBP_LOGD3("received %d bytes", (int) buffer_size);
    self->status.total_bytes += buffer_size;
    struct recv_buf_s buf = {
            .buf = buffer,
            .size = buffer_size,
    };
    recv(self, &buf);
}

void fbp_framer_reset(struct fbp_framer_s * self) {
    self->state = 0;
    self->is_sync = 0;
    self->length = 0;
    self->buf_offset = 0;
    fbp_memset(&self->status, 0, sizeof(self->status));
}

bool fbp_framer_validate_data(uint16_t frame_id, uint32_t metadata, uint32_t msg_size) {
    if ((msg_size < 1) || (msg_size > 256)) {
        return false;
    }
    if (frame_id > FBP_FRAMER_FRAME_ID_MAX) {
        return false;
    }
    if (metadata > FBP_FRAMER_MESSAGE_ID_MAX) {
        return false;
    }
    return true;
}

int32_t fbp_framer_construct_data(uint8_t * b, uint16_t frame_id, uint32_t metadata,
                                   uint8_t const *msg, uint32_t msg_size) {
    if (!fbp_framer_validate_data(frame_id, metadata, msg_size)) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    b[0] = FBP_FRAMER_SOF1;
    b[1] = FBP_FRAMER_SOF2;
    b[2] = (FBP_FRAMER_FT_DATA << 5) | ((frame_id >> 8) & 0x7);
    b[3] = msg_size - 1;
    b[4] = (uint8_t) (frame_id & 0xff);
    b[5] = metadata & 0xff;
    b[6] = (metadata >> 8) & 0xff;
    b[7] = (metadata >> 16) & 0xff;
    memcpy(b + FBP_FRAMER_HEADER_SIZE, msg, msg_size);
    uint32_t crc = fbp_crc32(0, b + 2, msg_size + FBP_FRAMER_HEADER_SIZE - 2);
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 0] = crc & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 1] = (crc >> 8) & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 2] = (crc >> 16) & 0xff;
    b[FBP_FRAMER_HEADER_SIZE + msg_size + 3] = (crc >> 24) & 0xff;
    return 0;
}

bool fbp_framer_validate_link(enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    int32_t frame_type_i32 = (int32_t) frame_type;
    if ((frame_type_i32 < 0) | (frame_type_i32 >= 8)) {
        return false;
    }
    if (frame_id > FBP_FRAMER_FRAME_ID_MAX) {
        return false;
    }
    return true;
}

int32_t fbp_framer_construct_link(uint8_t * b, enum fbp_framer_type_e frame_type, uint16_t frame_id) {
    if (!fbp_framer_validate_link(frame_type, frame_id)) {
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
    b[0] = FBP_FRAMER_SOF1;
    b[1] = FBP_FRAMER_SOF2;
    b[2] = (frame_type << 5) | ((frame_id >> 8) & 0x7);
    b[3] = (uint8_t) (frame_id & 0xff);
    uint32_t crc = fbp_crc32(0, b + 2, 2);
    b[4] = crc & 0xff;
    b[5] = (crc >> 8) & 0xff;
    b[6] = (crc >> 16) & 0xff;
    b[7] = (crc >> 24) & 0xff;
    return 0;
}

int32_t fbp_framer_frame_id_subtract(uint16_t a, uint16_t b) {
    uint16_t c = (a - b) & FBP_FRAMER_FRAME_ID_MAX;
    if (c > (FBP_FRAMER_FRAME_ID_MAX / 2)) {
        return ((int32_t) c) - (FBP_FRAMER_FRAME_ID_MAX + 1);
    } else {
        return (int32_t) c;
    }
}
