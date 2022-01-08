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
#include "fitterbap/host/comm.h"
#include "fitterbap/host/uart.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/pubsub.h"
#include "fitterbap/comm/stack.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/ec.h"
#include "fitterbap/cdef.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include <stdio.h>
#include <windows.h>


// On a host computer, make plenty big
#define PUBSUB_BUFFER_SIZE (2000000)
#define PUBSUB_PREFIX "h/"
#define STACK_PREFIX PUBSUB_PREFIX "c/"


struct fbp_comm_s {
    struct fbp_stack_s * stack;
    struct fbp_uart_s * uart;
    struct fbp_evm_s * evm;
    struct fbp_evm_api_s evm_api;

    fbp_os_mutex_t uart_mutex;
    HANDLE uart_signal_event;
    int uart_quit;
    HANDLE uart_thread;

    struct fbp_pubsub_s * pubsub;
    fbp_os_mutex_t pubsub_mutex;
    HANDLE pubsub_signal_event;
    int pubsub_quit;
    HANDLE pubsub_thread;

    fbp_pubsub_subscribe_fn subscriber_fn;
    void * subscriber_user_data;
};

static void print_buffer(const char * prefix, uint8_t const * buffer, uint32_t buffer_size) {
    if (FBP_LOG_CHECK_STATIC(FBP_LOG_LEVEL_DEBUG1)) {
        char line_buf[80];
        char * p = line_buf;
        for (uint32_t i = 0; i < buffer_size; ++i) {
            if (0 == (i & 0xf)) {
                if (i != 0) {
                    FBP_LOGD1("%s:%s", prefix, line_buf);
                }
                line_buf[0] = 0;
                p = line_buf;
            }
            int count = snprintf(p, sizeof(line_buf) - (line_buf - p), " %02x", buffer[i]);
            p += count;
        }

        if (p != line_buf) {
            FBP_LOGD1("%s:%s", prefix, line_buf);
        }
    }
}

static void ll_send(void * user_data, uint8_t const * buffer, uint32_t buffer_size) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    if (FBP_LOG_CHECK_STATIC(FBP_LOG_LEVEL_DEBUG3)) {
        print_buffer("s", buffer, buffer_size);
    }
    fbp_uart_write(self->uart, buffer, buffer_size);
}

static uint32_t ll_send_available(void * user_data) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    return fbp_uart_write_available(self->uart);
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    if (FBP_LOG_CHECK_STATIC(FBP_LOG_LEVEL_DEBUG3)) {
        print_buffer("r", buffer, buffer_size);
    }
    fbp_dl_ll_recv(self->stack->dl, buffer, buffer_size);
}

static void on_publish_fn(void * user_data) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    SetEvent(self->pubsub_signal_event);
}

static void on_port_recv_default(void *user_data,
                                 uint8_t port_id,
                                 enum fbp_transport_seq_e seq,
                                 uint8_t port_data,
                                 uint8_t *msg, uint32_t msg_size) {
    (void) port_data;
    char topic[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    if (msg_size > FBP_FRAMER_PAYLOAD_MAX_SIZE) {
        return;
    }

    // Construct topic string
    char * src = STACK_PREFIX;
    char * t = topic;
    while (*src) {
        *t++ = *src++;
    }
    if (port_id >= 10) {
        *t++ = '0' + (port_id / 10);
    }
    *t++ = '0' + (port_id % 10);
    src = "/din";
    while (*src) {
        *t++ = *src++;
    }
    *t = 0;

    if (seq != FBP_TRANSPORT_SEQ_SINGLE) {
        FBP_LOGW("unexpected seq %d on port %d", (int) seq, (int) port_id);
    }
    fbp_pubsub_publish(self->pubsub, topic, &fbp_union_bin(msg, msg_size), NULL, NULL);
}

static void on_schedule(void * user_data, int64_t next_time) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    (void) next_time;
    SetEvent(self->uart_signal_event);
}

static DWORD WINAPI uart_task(LPVOID lpParam) {
    int64_t t_now;
    int64_t t_dl;
    int64_t t_evm;
    int64_t t_next;
    int64_t t_duration;
    DWORD duration_ms = 0;
    uint32_t handle_count;
    HANDLE handles[16];
    struct fbp_comm_s * self = (struct fbp_comm_s *) lpParam;

    while (!self->uart_quit) {
        handle_count = FBP_ARRAY_SIZE(handles);
        fbp_uart_handles(self->uart, &handle_count, handles);
        handles[handle_count++] = self->uart_signal_event;
        WaitForMultipleObjects(handle_count, handles, FALSE, duration_ms);
        ResetEvent(self->uart_signal_event);

        t_now = fbp_time_rel();
        fbp_uart_process_read(self->uart);
        fbp_uart_process_write_completed(self->uart);
        t_dl = fbp_dl_process(self->stack->dl, t_now);
        fbp_evm_process(self->evm, fbp_time_rel());
        fbp_uart_process_write_pend(self->uart);
        t_evm = fbp_evm_time_next(self->evm);

        t_next = fbp_time_max(t_now, fbp_time_min(t_dl, t_evm));
        t_duration = t_next - t_now;
        if (t_duration > FBP_TIME_SECOND) {
            duration_ms = 1000;
        } else {
            duration_ms = (DWORD) FBP_TIME_TO_COUNTER(t_next - t_now, 1000);
        }
        duration_ms = 1;
    }
    return 0;
}

static int32_t uart_task_start(struct fbp_comm_s * self) {
    if (self->uart_thread) {
        FBP_LOGW("uart_task_start but already running");
        return FBP_ERROR_BUSY;
    }

    self->uart_thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            uart_task,              // thread function name
            self,                   // argument to thread function
            0,                      // use default creation flags
            NULL);                  // returns the thread identifier
    if (!self->uart_thread) {
        return FBP_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!SetThreadPriority(self->uart_thread, THREAD_PRIORITY_TIME_CRITICAL)) {
        FBP_LOGE("Could not elevate thread priority: %d", (int) GetLastError());
    }
    return 0;
}

static int32_t uart_task_stop(struct fbp_comm_s * self) {
    int rc = 0;
    if (self) {
        self->uart_quit = 1;
        SetEvent(self->uart_signal_event);
        if (self->uart_thread) {
            if (WAIT_OBJECT_0 != WaitForSingleObject(self->uart_thread, 1000)) {
                FBP_LOGW("UART thread failed to shut down gracefully");
                rc = FBP_ERROR_TIMED_OUT;
            }
            CloseHandle(self->uart_thread);
            self->uart_thread = NULL;
        }
    }
    return rc;
}

static DWORD WINAPI pubsub_task(LPVOID lpParam) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) lpParam;
    while (!self->pubsub_quit) {
        WaitForSingleObject(self->pubsub_signal_event, 1000);
        ResetEvent(self->pubsub_signal_event);
        fbp_pubsub_process(self->pubsub);
    }
    return 0;
}

static int32_t pubsub_task_start(struct fbp_comm_s * self) {
    if (self->pubsub_thread) {
        FBP_LOGW("pubsub_task_start but already running");
        return FBP_ERROR_BUSY;
    }

    self->pubsub_thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            pubsub_task,            // thread function name
            self,                   // argument to thread function
            0,                      // use default creation flags
            NULL);                  // returns the thread identifier
    if (!self->pubsub_thread) {
        FBP_LOGE("Could not start pubsub thread: %d", (int) GetLastError());
        return FBP_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!SetThreadPriority(self->pubsub_thread, THREAD_PRIORITY_ABOVE_NORMAL)) {
        FBP_LOGE("Could not elevate thread priority: %d", (int) GetLastError());
    }
    return 0;
}

static int32_t pubsub_task_stop(struct fbp_comm_s * self) {
    int rc = 0;
    if (self) {
        self->pubsub_quit = 1;
        SetEvent(self->pubsub_signal_event);
        if (self->pubsub_thread) {
            if (WAIT_OBJECT_0 != WaitForSingleObject(self->pubsub_thread, 1000)) {
                FBP_LOGW("UART thread failed to shut down gracefully");
                rc = FBP_ERROR_TIMED_OUT;
            }
            CloseHandle(self->pubsub_thread);
            self->pubsub_thread = NULL;
        }
    }
    return rc;
}

static void on_dl_process_request(void * user_data) {
    struct fbp_comm_s * self = (struct fbp_comm_s *) user_data;
    SetEvent(self->uart_signal_event);
}

struct fbp_comm_s * fbp_comm_initialize(struct fbp_dl_config_s const * config,
                                        const char * device,
                                        uint32_t baudrate,
                                        fbp_pubsub_subscribe_fn cbk_fn,
                                        void * cbk_user_data) {
    FBP_LOGI("fbp_comm_initialize(%s, %d)", device, (int) baudrate);
    if (!cbk_fn) {
        FBP_LOGW("Must provide cbk_fn");
        return NULL;
    }
    struct fbp_comm_s * self = fbp_alloc_clr(sizeof(struct fbp_comm_s));
    if (!self) {
        return NULL;
    }

    self->subscriber_fn = cbk_fn;
    self->subscriber_user_data = cbk_user_data;
    self->pubsub = fbp_pubsub_initialize(PUBSUB_PREFIX, PUBSUB_BUFFER_SIZE);
    if (!self->pubsub) {
        goto on_error;
    }

    self->pubsub_quit = 0;
    self->pubsub_signal_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!self->pubsub_signal_event) {
        goto on_error;
    }

    self->pubsub_mutex = fbp_os_mutex_alloc("pubsub");
    if (!self->pubsub_mutex) {
        goto on_error;
    }
    fbp_pubsub_register_mutex(self->pubsub, self->pubsub_mutex);
    fbp_pubsub_subscribe(self->pubsub, "",
                         FBP_PUBSUB_SFLAG_RETAIN | FBP_PUBSUB_SFLAG_RSP,
                         self->subscriber_fn, self->subscriber_user_data);

    self->uart_signal_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!self->uart_signal_event) {
        goto on_error;
    }

    self->uart_mutex = fbp_os_mutex_alloc("uart");
    if (!self->uart_mutex) {
        goto on_error;
    }

    self->evm = fbp_evm_allocate();
    fbp_evm_register_schedule_callback(self->evm, on_schedule, self);
    fbp_evm_register_mutex(self->evm, self->uart_mutex);
    FBP_ASSERT(0 == fbp_evm_api_get(self->evm, &self->evm_api));

    struct fbp_dl_ll_s ll = {
            .user_data = self,
            .send = ll_send,
            .send_available = ll_send_available,
    };

    struct fbp_uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_buffer_size = (FBP_FRAMER_MAX_SIZE + 4) / 2,
            .send_buffer_count = 8,
            .recv_buffer_size = FBP_FRAMER_MAX_SIZE,
            .recv_buffer_count = 16,
            .recv_fn = on_uart_recv,
            .recv_user_data = self,
    };

    self->uart = fbp_uart_alloc();
    if (!self->uart) {
        goto on_error;
    }
    if (fbp_uart_open(self->uart, device, &uart_config)) {
        goto on_error;
    }

    fbp_pubsub_register_on_publish(self->pubsub, on_publish_fn, self);

    self->stack = fbp_stack_initialize(config, FBP_PORT0_MODE_SERVER, STACK_PREFIX,
                                       &self->evm_api, &ll, self->pubsub, NULL);
    if (!self->stack) {
        goto on_error;
    }
    fbp_stack_mutex_set(self->stack, self->uart_mutex);
    fbp_transport_port_register_default(self->stack->transport, NULL, on_port_recv_default, self);
    fbp_dl_register_process_request(self->stack->dl, on_dl_process_request, self);

    if (uart_task_start(self)) {
        goto on_error;
    }
    if (pubsub_task_start(self)) {
        goto on_error;
    }

    return self;

on_error:
    FBP_LOGE("fbp_comm_initialize failed");
    fbp_comm_finalize(self);
    return NULL;
}

void fbp_comm_finalize(struct fbp_comm_s * self) {
    if (self) {
        pubsub_task_stop(self);
        uart_task_stop(self);
        if (self->stack) {
            fbp_stack_finalize(self->stack);
            self->stack = NULL;
        }
        if (self->uart) {
            fbp_uart_close(self->uart);
            fbp_uart_free(self->uart);
            self->uart = NULL;
        }
        if (self->uart_mutex) {
            fbp_os_mutex_free(self->uart_mutex);
            self->uart_mutex = NULL;
        }
        if (self->uart_signal_event) {
            CloseHandle(self->uart_signal_event);
            self->uart_signal_event = NULL;
        }
        if (self->pubsub) {
            fbp_pubsub_finalize(self->pubsub);
            self->pubsub = NULL;
        }
        if (self->pubsub_mutex) {
            fbp_os_mutex_free(self->pubsub_mutex);
            self->pubsub_mutex = NULL;
        }
        if (self->pubsub_signal_event) {
            CloseHandle(self->pubsub_signal_event);
            self->pubsub_signal_event = NULL;
        }
        fbp_free(self);
    }
}

int32_t fbp_comm_publish(struct fbp_comm_s * self,
                          const char * topic, const struct fbp_union_s * value) {
    FBP_LOGD1("publish(topic=%s, value.type=%d, value.size=%d)",
              topic, (int) value->type, (int) value->size);
    return fbp_pubsub_publish(self->pubsub, topic, value, self->subscriber_fn, self->subscriber_user_data);
}

int32_t fbp_comm_query(struct fbp_comm_s * self, const char * topic, struct fbp_union_s * value) {
    return fbp_pubsub_query(self->pubsub, topic, value);
}

int32_t fbp_comm_status_get(
        struct fbp_comm_s * self,
        struct fbp_dl_status_s * status) {
    return fbp_dl_status_get(self->stack->dl, status);
}

void fbp_comm_log_recv_register(struct fbp_comm_s * self, fbp_logp_publish_formatted fn, void * user_data) {
    fbp_logp_handler_register(self->stack->logp, fn, user_data);
}
