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

#include "fitterbap/event_manager.h"
#include "fitterbap/time.h"

struct fbp_transport_s {
    int32_t dummy;
};

struct fbp_pubsub_s {
    int32_t dummy;
};

struct test_s {
    struct fbp_pubsubp_s * s;
    struct fbp_transport_s t;
    struct fbp_pubsub_s p;
    struct fbp_evm_s * evm;
    struct fbp_evm_api_s evm_api;
    struct fbp_time_counter_s time_counter;
};

struct test_s * instance_;

struct test_s * port_hal_initialize() {
    struct test_s * self = fbp_alloc_clr(sizeof(struct test_s));
    self->time_counter.value = 0;
    self->time_counter.frequency = 1000;
    instance_ = self;
    self->evm = fbp_evm_allocate();
    assert_int_equal(0, fbp_evm_api_get(self->evm, &self->evm_api));
    return self;
}

void port_hal_finalize(struct test_s * self) {
    if (self) {
        if (self->evm) {
            fbp_evm_free(self->evm);
        }
        self->evm = NULL;
        memset(&self->evm_api, 0, sizeof(self->evm_api));
        fbp_free(self);
    }
    instance_ = NULL;
}

void fbp_os_mutex_lock(fbp_os_mutex_t mutex) {
    (void) mutex;
}

void fbp_os_mutex_unlock(fbp_os_mutex_t mutex) {
    (void) mutex;
}

void fbp_os_sleep(int64_t duration) {
    (void) duration;
}

struct fbp_time_counter_s fbp_time_counter() {
    return instance_->time_counter;
}

const char * fbp_pubsub_topic_prefix(struct fbp_pubsub_s * self) {
    (void) self;
    return "s";
}

int32_t fbp_pubsub_subscribe(struct fbp_pubsub_s * self, const char * topic,
                             uint8_t flags,
                             fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    (void) self;
    check_expected_ptr(topic);
    check_expected(flags);
    (void) cbk_fn;
    (void) cbk_user_data;
    return 0;
}

#define expect_subscribe(_topic, _flags)                    \
    expect_string(fbp_pubsub_subscribe, topic, _topic);     \
    expect_value(fbp_pubsub_subscribe, flags, _flags)

#define expect_any_subscribe()                    \
    expect_any(fbp_pubsub_subscribe, topic);     \
    expect_any(fbp_pubsub_subscribe, flags)

int32_t fbp_pubsub_unsubscribe(struct fbp_pubsub_s * self, const char * topic,
                               fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    (void) self;
    check_expected_ptr(topic);
    (void) cbk_fn;
    (void) cbk_user_data;
    return 0;
}

#define expect_unsubscribe(_topic)                            \
    expect_string(fbp_pubsub_unsubscribe, topic, _topic)

int32_t fbp_pubsub_unsubscribe_from_all(struct fbp_pubsub_s * self,
                                        fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    uint16_t value = 0;
    (void) self;
    (void) cbk_fn;
    (void) cbk_user_data;
    check_expected(value);
    return 0;
}

#define expect_unsubscribe_from_all()                            \
    expect_value(fbp_pubsub_unsubscribe_from_all, value, 0)

int32_t fbp_pubsub_meta(struct fbp_pubsub_s * self, const char * topic, const char * meta_json) {
    (void) self;
    check_expected_ptr(topic);
    (void) meta_json;
    return 0;
}

#define expect_meta(_topic)                            \
    expect_string(fbp_pubsub_meta, topic, _topic);

int32_t fbp_pubsub_publish(
        struct fbp_pubsub_s * self,
        const char * topic, const struct fbp_union_s * value,
        fbp_pubsub_subscribe_fn src_fn, void * src_user_data) {
    (void) self;
    (void) src_fn;
    (void) src_user_data;
    int type = value->type;
    uint32_t size = value->size;
    check_expected_ptr(topic);
    check_expected(type);
    const char * cstr = value->value.str;
    const uint8_t * bin = value->value.bin;
    uint32_t f32 = *((uint32_t *) &value->value.f32);
    uint64_t f64 = *((uint64_t *) &value->value.f64);
    uint8_t u8 = value->value.u8;
    uint16_t u16 = value->value.u16;
    uint32_t u32 = value->value.u32;
    uint64_t u64 = value->value.u64;
    int8_t i8 = value->value.i8;
    int16_t i16 = value->value.i16;
    int32_t i32 = value->value.i32;
    int64_t i64 = value->value.i64;
    switch (type) {
        case FBP_UNION_NULL: break;
        case FBP_UNION_STR: check_expected_ptr(cstr); break;
        case FBP_UNION_JSON: check_expected_ptr(cstr); break;
        case FBP_UNION_BIN: check_expected(size); check_expected_ptr(bin); break;
        case FBP_UNION_F32: check_expected(f32); break;
        case FBP_UNION_F64: check_expected(f64); break;
        case FBP_UNION_U8: check_expected(u8); break;
        case FBP_UNION_U16: check_expected(u16); break;
        case FBP_UNION_U32: check_expected(u32); break;
        case FBP_UNION_U64: check_expected(u64); break;
        case FBP_UNION_I8: check_expected(i8); break;
        case FBP_UNION_I16: check_expected(i16); break;
        case FBP_UNION_I32: check_expected(i32); break;
        case FBP_UNION_I64: check_expected(i64); break;
        default: check_expected(self); break; // not valid
    }
    return 0;
}

#define expect_publish_null(_topic)                  \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_NULL)

#define expect_publish_str(_topic, _cstr)                  \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_STR);  \
    expect_string(fbp_pubsub_publish, cstr, _cstr);

#define expect_publish_json(_topic, _cstr)                  \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_JSON);  \
    expect_string(fbp_pubsub_publish, cstr, _cstr);

#define expect_publish_bin(_topic, _bin, _len)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_BIN);\
    expect_value(fbp_pubsub_publish, size, _len);  \
    expect_memory(fbp_pubsub_publish, bin, _bin, _len);

#define expect_publish_f32(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_F32);  \
    expect_value(fbp_pubsub_publish, f32, *((uint32_t *) &_v))

#define expect_publish_f64(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_F64);  \
    expect_value(fbp_pubsub_publish, f64, *((uint64_t *) &_v))

#define expect_publish_u8(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_U8);  \
    expect_value(fbp_pubsub_publish, u8, _v)

#define expect_publish_u16(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_U16);  \
    expect_value(fbp_pubsub_publish, u16, _v)

#define expect_publish_u32(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_U32);  \
    expect_value(fbp_pubsub_publish, u32, _v)

#define expect_publish_u64(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_U64);  \
    expect_value(fbp_pubsub_publish, u64, _v)

#define expect_publish_i8(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_I8);  \
    expect_value(fbp_pubsub_publish, i8, _v)

#define expect_publish_i16(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_I16);  \
    expect_value(fbp_pubsub_publish, i16, _v)

#define expect_publish_i32(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_I32);  \
    expect_value(fbp_pubsub_publish, i32, _v)

#define expect_publish_i64(_topic, _v)                    \
    expect_string(fbp_pubsub_publish, topic, _topic);               \
    expect_value(fbp_pubsub_publish, type, FBP_UNION_I64);  \
    expect_value(fbp_pubsub_publish, i64, _v)


int32_t fbp_pubsub_query(struct fbp_pubsub_s * self, const char * topic, struct fbp_union_s * value) {
    (void) self;
    check_expected_ptr(topic);
    value->type = mock_type(uint8_t);
    value->value.u64 = 0;
    value->size = mock_type(uint32_t);
    switch (value->type) {
        case FBP_UNION_NULL: break;
        case FBP_UNION_STR: value->value.str = mock_ptr_type(char *); break;
        case FBP_UNION_JSON: value->value.str = mock_ptr_type(char *); break;
        case FBP_UNION_BIN: value->value.bin = mock_ptr_type(uint8_t *); break;
        case FBP_UNION_F32: value->value.f32 = mock_type(float); break;
        case FBP_UNION_F64: value->value.f64 = mock_type(double); break;
        case FBP_UNION_U8: value->value.u8 = mock_type(uint8_t); break;
        case FBP_UNION_U16: value->value.u16 = mock_type(uint16_t); break;
        case FBP_UNION_U32: value->value.u32 = mock_type(uint32_t); break;
        case FBP_UNION_U64: value->value.u64 = mock_type(uint64_t); break;
        case FBP_UNION_I8: value->value.i8 = mock_type(int8_t); break;
        case FBP_UNION_I16: value->value.i16 = mock_type(int16_t); break;
        case FBP_UNION_I32: value->value.i32 = mock_type(int32_t); break;
        case FBP_UNION_I64: value->value.i64 = mock_type(int64_t); break;
        default: assert_true(0); break;
    }
    return 0;
}

#define expect_query_str(_topic, _str) \
    expect_string(fbp_pubsub_query, topic, _topic); \
    will_return(fbp_pubsub_query, FBP_UNION_STR);         \
    will_return(fbp_pubsub_query, strlen(_str) + 1);      \
    will_return(fbp_pubsub_query, (intptr_t)(_str))

int32_t fbp_transport_send(struct fbp_transport_s * self,
                           uint8_t port_id,
                           enum fbp_transport_seq_e seq,
                           uint8_t port_data,
                           uint8_t const *msg, uint32_t msg_size,
                           uint32_t timeout_ms) {
    (void) self;
    check_expected(port_id);
    check_expected(seq);
    check_expected(port_data);
    check_expected(msg_size);
    check_expected_ptr(msg);
    check_expected(timeout_ms);
    return 0;
}

#define expect_send(_port_id, _port_data, _msg, _msg_size, _timeout_ms)     \
    expect_value(fbp_transport_send, port_id, _port_id);                    \
    expect_value(fbp_transport_send, seq, FBP_TRANSPORT_SEQ_SINGLE);        \
    expect_value(fbp_transport_send, port_data, _port_data);                \
    expect_value(fbp_transport_send, msg_size, _msg_size);                  \
    expect_memory(fbp_transport_send, msg, _msg, _msg_size);                \
    expect_value(fbp_transport_send, timeout_ms, _timeout_ms)

int32_t fbp_transport_port_register(struct fbp_transport_s * self,
                                    uint8_t port_id,
                                    const char * meta,
                                    fbp_transport_event_fn event_fn,
                                    fbp_transport_recv_fn recv_fn,
                                    void * user_data) {
    (void) self;
    (void) meta;
    check_expected_ptr(port_id);
    (void) event_fn;
    (void) recv_fn;
    (void) user_data;
    return 0;
}

void fbp_transport_event_inject(struct fbp_transport_s * self, enum fbp_dl_event_e event) {
    (void) self;
    check_expected(event);
}

#define expect_inject(event_) \
    expect_value(fbp_transport_event_inject, event, event_)
