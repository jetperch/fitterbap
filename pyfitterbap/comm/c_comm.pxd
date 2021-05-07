# Copyright 2020-2021 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from libc.stdint cimport int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t


cdef extern from "fitterbap/comm/framer.h":
    struct fbp_framer_status_s:
        uint64_t total_bytes
        uint64_t ignored_bytes
        uint64_t resync

cdef extern from "fitterbap/union.h":
    enum fbp_union_e:
        FBP_UNION_NULL = 0
        FBP_UNION_STR = 1
        FBP_UNION_JSON = 2
        FBP_UNION_BIN = 3
        FBP_UNION_F32 = 6
        FBP_UNION_F64 = 7
        FBP_UNION_U8 = 8
        FBP_UNION_U16 = 9
        FBP_UNION_U32 = 10
        FBP_UNION_U64 = 11
        FBP_UNION_I8 = 12
        FBP_UNION_I16 = 13
        FBP_UNION_I32 = 14
        FBP_UNION_I64 = 15

    enum fbp_union_flag_e:
        FBP_UNION_FLAG_NONE = 0
        FBP_UNION_FLAG_CONST = (1 << 0)
        FBP_UNION_FLAG_RETAIN = (1 << 1)

    union fbp_union_inner_u:
        const char * str
        const uint8_t * bin
        float f32
        double f64
        uint8_t u8
        uint16_t u16
        uint32_t u32
        uint64_t u64
        int8_t i8
        int16_t i16
        int32_t i32
        int64_t i64

    struct fbp_union_s:
        uint8_t type
        uint8_t flags
        uint8_t op
        uint8_t app
        fbp_union_inner_u value
        uint32_t size


cdef extern from "fitterbap/pubsub.h":

    enum fbp_pubsub_sflag_e:
        FBP_PUBSUB_SFLAG_NONE = 0
        FBP_PUBSUB_SFLAG_RETAIN = (1 << 0)
        FBP_PUBSUB_SFLAG_NOPUB = (1 << 1)
        FBP_PUBSUB_SFLAG_REQ = (1 << 2)
        FBP_PUBSUB_SFLAG_RSP = (1 << 3)

    ctypedef uint8_t (*fbp_pubsub_subscribe_fn)(void * user_data,
            const char * topic, const fbp_union_s * value)


cdef extern from "fitterbap/comm/data_link.h":

    struct fbp_dl_config_s:
        uint32_t tx_link_size
        uint32_t tx_window_size
        uint32_t tx_buffer_size
        uint32_t rx_window_size
        uint32_t tx_timeout

    struct fbp_dl_tx_status_s:
        uint64_t bytes
        uint64_t msg_bytes
        uint64_t data_frames
        uint64_t retransmissions

    struct fbp_dl_rx_status_s:
        uint64_t msg_bytes
        uint64_t data_frames

    struct fbp_dl_status_s:
        uint32_t version
        uint32_t reserved
        fbp_dl_rx_status_s rx
        fbp_framer_status_s rx_framer
        fbp_dl_tx_status_s tx

    enum fbp_dl_event_e:
        FBP_DL_EV_UNKNOWN
        FBP_DL_EV_RX_RESET_REQUEST
        FBP_DL_EV_TX_DISCONNECTED
        FBP_DL_EV_INTERNAL_ERROR

    struct fbp_dl_api_s:
        void *user_data
        void (*event_fn)(void *user_data, fbp_dl_event_e event) nogil
        void (*recv_fn)(void *user_data, uint32_t metadata,
                        uint8_t *msg, uint32_t msg_size) nogil


cdef extern from "fitterbap/host/comm.h":
    struct fbp_comm_s
    fbp_comm_s * fbp_comm_initialize(const fbp_dl_config_s * config,
                                     const char * device,
                                     uint32_t baudrate,
                                     fbp_pubsub_subscribe_fn cbk_fn,
                                     void * cbk_user_data)
    void fbp_comm_finalize(fbp_comm_s * self)
    int32_t fbp_comm_publish(fbp_comm_s * self,
                             const char * topic, const fbp_union_s * value)
    int32_t fbp_comm_query(fbp_comm_s * self, const char * topic, fbp_union_s * value)
    int32_t fbp_comm_status_get(fbp_comm_s * self, fbp_dl_status_s * status)
