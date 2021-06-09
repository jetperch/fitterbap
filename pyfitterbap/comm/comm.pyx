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


from .c_comm cimport *
import numpy as np
cimport numpy as np
include "../module.pxi"
import json
import logging


# From fitterbap/time.h
FBP_TIME_Q = 30
FBP_TIME_SECOND = (1 << FBP_TIME_Q)
FBP_TIME_MILLISECOND = ((FBP_TIME_SECOND + 500) // 1000)
FBP_TIME_EPOCH_UNIX_OFFSET_SECONDS = 1514764800

log = logging.getLogger(__name__)
TX_TIMEOUT_DEFAULT = 16 * FBP_TIME_MILLISECOND
LOG_TOPIC = 'h/log/msg'


cdef _value_pack(fbp_union_s * value, data, retain=None):
    s = None
    if data is None:
        value[0].type = FBP_UNION_NULL
        value[0].size = 0
    elif isinstance(data, int):
        value[0].type = FBP_UNION_U32
        value[0].value.u32 = data
        value[0].size = 4
    elif isinstance(data, str):
        s = data.encode('utf-8')
        value[0].type = FBP_UNION_STR
        value[0].value.str = s
        value[0].size = <uint32_t> (len(s) + 1)
    elif isinstance(data, bytes):
        value[0].type = FBP_UNION_BIN
        value[0].value.bin = data
        value[0].size = <uint32_t> len(s)
    else:
        s = json.dumps(data).encode('utf-8')
        value[0].type = FBP_UNION_JSON
        value[0].value.str = s
        value[0].size = <uint32_t> (len(s) + 1)
    log.info("_value_pack(type=%d, size=%d)", value[0].type, value[0].size)
    if bool(retain):
        value[0].flags |= FBP_UNION_FLAG_RETAIN
    return s  # so that caller can keep valid until used.


cdef _value_unpack(const fbp_union_s * value):
    retain = (0 != (value[0].flags & FBP_UNION_FLAG_RETAIN))
    dtype = value[0].type & 0x0f
    if dtype == FBP_UNION_NULL:
        v = None
    elif dtype == FBP_UNION_STR:
        if value[0].size <= 1:
            v = None
        else:
            v = value[0].value.str[:(value[0].size - 1)].decode('utf-8')
    elif dtype == FBP_UNION_BIN:
        v = value[0].value.bin[:value[0].size]
    elif dtype == FBP_UNION_JSON:
        if value[0].size <= 1:
            v = None
        else:
            s = value[0].value.str[:(value[0].size - 1)].decode('utf-8')
            try:
                v = json.loads(s)
            except Exception:
                log.warning('JSON loads failed: %s', s)
                v = None
    elif dtype == FBP_UNION_F32:
        v = value[0].value.f32
    elif dtype == FBP_UNION_F64:
        v = value[0].value.f64
    elif dtype == FBP_UNION_U8:
        v = value[0].value.u8
    elif dtype == FBP_UNION_U16:
        v = value[0].value.u16
    elif dtype == FBP_UNION_U32:
        v = value[0].value.u32
    elif dtype == FBP_UNION_U64:
        v = value[0].value.u64
    elif dtype == FBP_UNION_I8:
        v = value[0].value.i8
    elif dtype == FBP_UNION_I16:
        v = value[0].value.i16
    elif dtype == FBP_UNION_I32:
        v = value[0].value.i32
    elif dtype == FBP_UNION_I64:
        v = value[0].value.i64
    else:
        raise RuntimeError(f'Unsupported value type: {dtype}')
    return v, retain


cdef _dl_status_decode(fbp_dl_status_s * status):
    return {
        'version': status[0].version,
        'rx': {
            'msg_bytes': status[0].rx.msg_bytes,
            'data_frames': status[0].rx.data_frames,
        },
        'rx_framer': {
            'total_bytes': status[0].rx_framer.total_bytes,
            'ignored_bytes': status[0].rx_framer.ignored_bytes,
            'resync': status[0].rx_framer.resync,
        },
        'tx': {
            'bytes': status[0].tx.bytes,
            'msg_bytes': status[0].tx.msg_bytes,
            'data_frames': status[0].tx.data_frames,
            'retransmissions': status[0].tx.retransmissions,
        },
    }


class Port0Events:
    UNKNOWN = FBP_DL_EV_UNKNOWN
    RESET_REQUEST = FBP_DL_EV_RESET_REQUEST
    DISCONNECTED = FBP_DL_EV_DISCONNECTED
    CONNECTED = FBP_DL_EV_CONNECTED
    TRANSPORT_CONNECTED = FBP_DL_EV_TRANSPORT_CONNECTED
    APP_CONNECTED = FBP_DL_EV_APP_CONNECTED


cdef class Comm:
    """A Communication Device using the FBP stack.

    :param device: The device string.
    :param subscriber: The subscriber callback(topic, value, retain, src_cbk)
        for topic updates.  This function is called from the comm thread
        and must return quickly.  The authors recommend posting to a queue
        and then handling the update in a separate thread.  Be aware that
        Qt has some series issues with resynchronization from non-Qt threads.
        See ui.resync for an example of how to resynchronize safely.
    :param baudrate: The baud rate for COM / UART ports.
    """

    cdef fbp_comm_s * _comm
    cdef object _subscriber
    cdef object _device

    def __init__(self, device: str, subscriber,
            baudrate=None,
            tx_link_size=None,
            tx_window_size=None,
            rx_window_size=None,
            tx_timeout=None):

        cdef fbp_dl_config_s config
        log.debug('Comm.__init__ start')
        self._subscriber = subscriber
        self._device = device

        baudrate = 3000000 if baudrate is None else int(baudrate)
        config.tx_link_size = 256 if tx_link_size is None else int(tx_link_size)
        config.tx_window_size = 256 if tx_window_size is None else int(tx_window_size)
        config.rx_window_size = 256 if rx_window_size is None else int(rx_window_size)
        config.tx_timeout = TX_TIMEOUT_DEFAULT if tx_timeout is None else int(tx_timeout)
        device_str = device.encode('utf-8')
        log.info('comm_initialize(%s, %s)', device_str, baudrate)
        self._comm = fbp_comm_initialize(&config, device_str, baudrate, Comm._subscriber_cbk, <void *> self)
        if not self._comm:
            raise RuntimeError('Could not allocate instance')
        fbp_comm_log_recv_register(self._comm, Comm._on_logp_recv, <void *> self)

    @staticmethod
    cdef uint8_t _subscriber_cbk(void * user_data, const char * topic, const fbp_union_s * value) with gil:
        cdef Comm self = <object> user_data
        v, retain = _value_unpack(value)
        topic_str = topic.decode('utf-8')
        try:
            self._subscriber(topic_str, v, retain, self.publish)
        except Exception:
            log.exception(f'_subscriber_cbk({topic})')

    @staticmethod
    cdef void _on_logp_recv(void * user_data, const fbp_logp_record_s * record) with gil:
        cdef Comm self = <object> user_data
        msg = {
            'timestamp': (record[0].timestamp / FBP_TIME_SECOND) + FBP_TIME_EPOCH_UNIX_OFFSET_SECONDS,
            'level': record[0].level,
            'device': self._device,
            'origin_prefix': record[0].origin_prefix,
            'origin_thread': record[0].origin_thread,
            'filename': record[0].filename.decode('utf-8'),
            'line': record[0].line,
            'message': record[0].message.decode('utf-8'),
        }
        try:
            self._subscriber(LOG_TOPIC, msg, 0, self.publish)
        except Exception:
            log.exception(f'_subscriber_cbk({LOG_TOPIC})')

    def close(self):
        fbp_comm_finalize(self._comm)

    def publish(self, topic: str, value, retain=None, src_cbk=None):
        # ignore src_cbk since already implemented in comm
        cdef int32_t rc
        cdef fbp_union_s v
        s = _value_pack(&v, value, retain)
        topic_str = topic.encode('utf-8')
        rc = fbp_comm_publish(self._comm, topic_str, &v)
        if rc:
            raise RuntimeError(f'publish({topic}) failed with {rc}')

    def query(self, topic):
        cdef int32_t rc
        cdef fbp_union_s v
        rc = fbp_comm_query(self._comm, topic, &v)
        if rc:
            raise RuntimeError(f'query({topic}) failed with {rc}')
        return _value_unpack(&v)

    def status(self):
        cdef fbp_dl_status_s status
        fbp_comm_status_get(self._comm, &status)
        return _dl_status_decode(&status)
