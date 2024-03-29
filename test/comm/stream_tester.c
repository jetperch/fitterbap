/*
 * Copyright 2020-2021 Jetperch LLC
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

#include "fitterbap/comm/data_link.h"
#include "fitterbap/comm/framer.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/event_manager.h"
#include "fitterbap/time.h"
#include "fitterbap/log.h"
#include "fitterbap/ec.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>

#define SEND_AVAILABLE (FBP_FRAMER_MAX_SIZE + 48)

struct msg_s {
    uint16_t metadata;
    uint8_t msg_buffer[FBP_FRAMER_MAX_SIZE + 1];  // hold messages and frames
    uint32_t msg_size;
    struct fbp_list_s item;
};

struct stream_tester_s;

struct host_s {
    char name;
    struct fbp_framer_s * framer;
    struct fbp_dl_s * udl;
    struct fbp_list_s recv_expect;
    struct fbp_list_s send_queue;
    struct stream_tester_s * stream_tester;
    struct host_s * target;
    uint16_t metadata;
    uint8_t connected;
};

struct stream_tester_s {
    uint64_t byte_drop_rate;
    uint64_t byte_insert_rate;
    uint64_t bit_error_rate;
    int64_t now;
    struct fbp_list_s msg_free;
    struct host_s a;
    struct host_s b;
};

struct stream_tester_s s_;

static uint64_t rand_u60() {
    return (((uint64_t) rand() & 0x7fff) << 45)
        | (((uint64_t) rand() & 0x7fff) << 30)
        | (((uint64_t) rand() & 0x7fff) << 15)
        | (((uint64_t) rand() & 0x7fff) << 0);
}

/*
static uint32_t rand_u30() {
    return (((uint32_t) rand() & 0x7fff) << 15)
           | (((uint32_t) rand() & 0x7fff) << 0);
}
*/

static inline uint16_t rand_u15() {
    return ((uint16_t) rand() & 0x7fff);
}

void fbp_fatal(char const * file, int line, char const * msg) {
    struct fbp_dl_status_s a_status;
    struct fbp_dl_status_s b_status;
    fbp_dl_status_get(s_.a.udl, &a_status);
    fbp_dl_status_get(s_.b.udl, &b_status);
    printf("FATAL: %s:%d: %s\n", file, line, msg);
    fflush(stdout);
    exit(1);
}

void * fbp_alloc_(fbp_size_t size_bytes) {
    return malloc((size_t) size_bytes);
}

void fbp_free_(void * ptr) {
    free(ptr);
}

void fbp_log_printf_(const char * format, ...) {
    va_list arg;
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

static struct msg_s * msg_alloc(struct stream_tester_s * self) {
    struct msg_s * msg;
    if (fbp_list_is_empty(&self->msg_free)) {
        msg = fbp_alloc_clr(sizeof(struct msg_s));
        FBP_ASSERT_ALLOC(msg);
        fbp_list_initialize(&msg->item);
    } else {
        struct fbp_list_s * item = fbp_list_remove_head(&self->msg_free);
        msg = FBP_CONTAINER_OF(item, struct msg_s, item);
    }
    return msg;
}

static void msg_free(struct stream_tester_s * self, struct msg_s * msg) {
    fbp_list_add_tail(&self->msg_free, &msg->item);
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct host_s * host = (struct host_s *) user_data;
    struct msg_s * msg = msg_alloc(host->stream_tester);
    FBP_ASSERT(buffer_size <= FBP_FRAMER_MAX_SIZE);
    msg->msg_size = buffer_size;
    memcpy(msg->msg_buffer, buffer, buffer_size);
    fbp_list_add_tail(&host->send_queue, &msg->item);
}

static uint32_t ll_send_available(void * user_data) {
    struct host_s * host = (struct host_s *) user_data;
    (void) host;
    return SEND_AVAILABLE;
}

static void on_event(void *user_data, enum fbp_dl_event_e event) {
    struct host_s * host = (struct host_s *) user_data;
    if (event == FBP_DL_EV_CONNECTED) {
        FBP_LOGI("on_event(%c, CONNECTED)", host->name);
        host->connected = 1;
        fbp_dl_tx_window_set(host->udl, fbp_dl_tx_window_max_get(host->udl));
    } else if (event > FBP_DL_EV_CONNECTED) {
        // ignore
    } else if (event == FBP_DL_EV_DISCONNECTED) {
        FBP_LOGW("on_event(%c, DISCONNECTED)", host->name);
        host->connected = 0;
        while (!fbp_list_is_empty(&host->recv_expect)) {
            struct fbp_list_s * item = fbp_list_remove_head(&host->recv_expect);
            struct msg_s * msg = FBP_CONTAINER_OF(item, struct msg_s, item);
            msg_free(host->stream_tester, msg);
        }
    }
}

static void on_recv(void *user_data, uint16_t metadata,
                uint8_t *msg, uint32_t msg_size) {
    struct host_s * host = (struct host_s *) user_data;
    FBP_ASSERT(!fbp_list_is_empty(&host->recv_expect));
    struct fbp_list_s * item = fbp_list_remove_head(&host->recv_expect);
    struct msg_s * msg_expect = FBP_CONTAINER_OF(item, struct msg_s, item);
    FBP_ASSERT(metadata == msg_expect->metadata);
    FBP_ASSERT(msg_size == msg_expect->msg_size);
    FBP_ASSERT(0 == memcmp(msg, msg_expect->msg_buffer, msg_size));
    msg_free(host->stream_tester, msg_expect);
}

static void host_initialize(struct host_s *host, struct stream_tester_s * parent,
        char name, struct host_s * target,
        struct fbp_dl_config_s const * config) {
    host->stream_tester = parent;
    host->name = name;
    host->target = target;
    host->framer = fbp_framer_initialize();
    struct fbp_dl_ll_s ll = {
            .user_data = host,
            .send = ll_send,
            .send_available = ll_send_available,
    };

    host->udl = fbp_dl_initialize(config, &ll, host->framer);
    FBP_ASSERT_ALLOC(host->udl);
    fbp_list_initialize(&host->recv_expect);
    fbp_list_initialize(&host->send_queue);

    struct fbp_dl_api_s ul = {
            .user_data = host,
            .event_fn = on_event,
            .recv_fn = on_recv,
    };
    fbp_dl_register_upper_layer(host->udl, &ul);
}

static void send(struct host_s *host) {
    struct msg_s * msg = msg_alloc(host->stream_tester);
    msg->msg_size = 1 + (rand() & 0xff);
    msg->metadata = host->metadata;
    // msg->metadata = rand() & 0x00ffffff;
    for (uint16_t idx = 0; idx < msg->msg_size; ++idx) {
        msg->msg_buffer[idx] = rand() & 0xff;
    }
    int32_t rv = fbp_dl_send(host->udl, msg->metadata, msg->msg_buffer, msg->msg_size);
    switch (rv) {
        case FBP_SUCCESS:
            ++host->metadata;
            fbp_list_add_tail(&host->target->recv_expect, &msg->item);
            return;
        case FBP_ERROR_FULL:
            // discard and wait, try with a new message at another time.
            break;
        case FBP_ERROR_UNAVAILABLE:
            // discard and wait until connect.
            break;
        default:
            FBP_LOGW("fbp_dl_send error %d: %s", (int) rv, fbp_error_code_description(rv));
            break;
    }
}

static void action(struct stream_tester_s * self) {
#if 0
    self->time_ms += 1 ; // rand() & 0x03;
    send(&self->a);
#else
    uint8_t action = rand() & 3;
    self->now += FBP_TIME_MICROSECOND * 100;
    switch (action) {
        case 0:
            break;
        case 1:
            send(&self->a);
            break;
        case 2:
            send(&self->b);
            break;
        case 3:
            break;
    }
#endif
}

static void process_host(struct host_s * host) {
    struct fbp_list_s * item;
    struct msg_s * msg;
    if (fbp_list_is_empty(&host->send_queue)) {
        return;
    }
    item = fbp_list_remove_head(&host->send_queue);
    msg = FBP_CONTAINER_OF(item, struct msg_s, item);

    if (FBP_LOG_CHECK_STATIC(FBP_LOG_LEVEL_DEBUG3)) {
        printf("%c [%d]: ", host->name, msg->msg_size);
        for (uint32_t i = 0; i < msg->msg_size; ++i) {
            printf(" %02x", msg->msg_buffer[i]);
        }
        printf("\n");
    }

    // Permute message
    //uint64_t r_byte_ins = rand_u64() % host->stream_tester->byte_insert_rate;
    //uint64_t r_bit_error = rand_u64() % host->stream_tester->bit_error_rate;

    if (host->stream_tester->byte_drop_rate) {
        while (msg->msg_size > 1) {
            uint64_t r = rand_u60() % host->stream_tester->byte_drop_rate;
            if (r > msg->msg_size) {
                break;  // do not drop any bytes this time
            }
            // Compute the dropped byte index
            uint16_t idx = rand_u15() % msg->msg_size;
            if ((idx + 1U) == msg->msg_size) {
                --msg->msg_size;
            } else {
                fbp_memcpy(msg->msg_buffer + idx, msg->msg_buffer + idx + 1, msg->msg_size - (idx + 1U));
            }
        }
    }

    if (host->stream_tester->byte_insert_rate) {
        while (msg->msg_size < FBP_FRAMER_MAX_SIZE) {
            uint64_t r = rand_u60() % host->stream_tester->byte_insert_rate;
            if (r > msg->msg_size) {
                break;  // do not insert this time
            }
            // Compute the inserted byte and index
            uint8_t b = (uint8_t) (rand_u15() & 0xff);
            uint16_t idx = rand_u15() % (msg->msg_size + 1);
            if (idx == msg->msg_size) {
                msg->msg_buffer[msg->msg_size] = b;
            } else {
                fbp_memcpy(msg->msg_buffer + idx + 1, msg->msg_buffer + idx, msg->msg_size - idx);
                msg->msg_buffer[idx] = b;
            }
            ++msg->msg_size;
        }
    }

    if (host->stream_tester->bit_error_rate) {
        while (1) {
            uint64_t r = rand_u60() % host->stream_tester->bit_error_rate;
            if (r > (msg->msg_size * 8)) {
                break;  // do not insert this time
            }
            // Compute the inserted byte and index
            uint8_t bit_idx = (uint8_t) (rand_u15() & 0x7);
            uint16_t byte_idx = rand_u15() % (msg->msg_size + 1);
            msg->msg_buffer[byte_idx] ^= (1 << bit_idx);
        }
    }

    fbp_dl_ll_recv(host->target->udl, msg->msg_buffer, msg->msg_size);
    msg_free(host->stream_tester, msg);
}

static void process(struct stream_tester_s * self) {
    int do_run = 1;
    while (do_run) {
        bool a_empty = fbp_list_is_empty(&self->a.send_queue);
        bool b_empty = fbp_list_is_empty(&self->b.send_queue);
        if (a_empty && b_empty) {
            do_run = 0; // nothing to do
        } else if (!a_empty && !b_empty) {
            // pick at random
            if (rand() & 1) {
                process_host(&self->b);
            } else {
                process_host(&self->a);
            }
        } else if (!a_empty) {
            process_host(&self->a);
        } else {
            process_host(&self->b);
        }

        // process the events
        fbp_dl_process(self->a.udl, self->now);
        fbp_dl_process(self->b.udl, self->now);
    }
}

void handle_tick() {
    struct fbp_dl_status_s a;
    struct fbp_dl_status_s b;
    fbp_dl_status_get(s_.a.udl, &a);
    fbp_dl_status_get(s_.b.udl, &b);
    printf("a_rx=%" PRId64 ", a_tx=%" PRId64 " b_rx=%" PRId64 ", b_tx=%" PRId64 "\n",
           a.rx.data_frames, a.tx.data_frames, b.rx.data_frames, b.tx.data_frames);
    fflush(stdout);
}

int main(void) {
    struct fbp_dl_config_s config = {
        .tx_window_size = 64,
        .rx_window_size = 64,
        .tx_timeout = 10 * FBP_TIME_MILLISECOND,
        .tx_link_size = 64,
    };

    // printf("RAND_MAX = %ull\n", RAND_MAX);
    srand(2);
    fbp_memset(&s_, 0, sizeof(s_));
    fbp_list_initialize(&s_.msg_free);
    host_initialize(&s_.a, &s_, 'a', &s_.b, &config);
    host_initialize(&s_.b, &s_, 'b', &s_.a, &config);

    s_.byte_drop_rate = 2000;
    s_.byte_insert_rate = 2000;
    s_.bit_error_rate = 10000;
    int counter = 0;
    int64_t sys_time_now = fbp_time_rel();
    int64_t sys_time_last = sys_time_now;

    // allow sides to connect
    while (!s_.a.connected && !s_.b.connected) {
        process(&s_);
        s_.now += FBP_TIME_MILLISECOND;
    }
    FBP_LOGI("Connected");
    fflush(stdout);

    while (1) {
        action(&s_);
        process(&s_);
        ++counter;
        if (counter > 1000) {
            sys_time_now = fbp_time_rel();
            if ((sys_time_now - sys_time_last) > FBP_TIME_SECOND) {
                handle_tick();
                sys_time_last += FBP_TIME_SECOND;
            }
            counter = 0;
        }
    }
}
