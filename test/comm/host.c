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

#include "fitterbap/host/uart.h"
#include "fitterbap/comm/stack.h"
#include "fitterbap/cdef.h"
#include "fitterbap/cstr.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

// NOTE: reduce latency timer to 1 milliseconds for FTDI chips.

struct host_s {
    // struct fbp_transport_s transport;
    struct fbp_uart_s * uart;
    struct fbp_stack_s * stack;
    struct fbp_pubsub_s * pubsub;

    HANDLE ctrl_event;
    volatile bool do_quit;
};

struct host_s h_;


static const char * enable_meta = "{"
    "\"dtype\": \"u32\","
    "\"brief\": \"Enable\","
    "\"default\": 0,"
    "\"options\": [[0, \"off\"], [1, \"on\"]]"
"}";


void fbp_fatal(char const * file, int line, char const * msg) {
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

void fbp_log_printf_(const char *format, ...) {
    va_list arg;
    printf("%d ", (uint32_t) fbp_time_rel_ms());
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    struct host_s * self = (struct host_s *) user_data;
    fbp_dl_ll_recv(self->stack->dl, buffer, buffer_size);
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    h_.do_quit = true;
    SetEvent(h_.ctrl_event);
    return TRUE;
}

#define ARG_CONSUME(count)   argc -= count; argv += count

static const char USAGE[] =
"usage: host.exe --port <port> --baudrate <baudrate> --client \n"
"    port: The COM port path, such as COM3\n"
"    baudrate: The baud rate\n"
"    client: Configure as client, not server.\n"
"\n";


uint8_t pubsub_sub(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) user_data;
    switch (value->type & 0x0f) {
        case FBP_UNION_U32:   printf("pubsub(%s, %" PRIu32 ")\n", topic, value->value.u32); break;
        case FBP_UNION_STR:   printf("pubsub(%s, %s)\n", topic, value->value.str); break;
        case FBP_UNION_JSON:  printf("pubsub(%s, %s)\n", topic, value->value.str); break;
        case FBP_UNION_BIN:   printf("pubsub(%s, bin %d)\n", topic, (int) value->size); break;
        default:
            printf("pubsub(%s, unknown %d)\n", topic, (int) value->size);
            break;
    }
    return 0;
}

uint8_t enable_sub(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) user_data;
    (void) topic;
    bool b = false;
    if (fbp_union_to_bool(value, &b)) {
        printf("enable invalid value\n");
    } else {
        printf("enable %s\n", b ? "on" : "off");
    }
    return 0;
}

int main(int argc, char * argv[]) {
    uint32_t baudrate = 3000000;
    uint32_t pubsub_buffer_size = 1000000;
    char device_path[1024];
    struct fbp_topic_s topic;
    char pubsub_prefix[FBP_PUBSUB_TOPIC_LENGTH_MAX] = "h/";
    char port0_prefix[FBP_PUBSUB_TOPIC_LENGTH_MAX] = "h/c/";
    enum fbp_port0_mode_e port0_mode = FBP_PORT0_MODE_SERVER;
    struct fbp_dl_config_s dl_config = {
            .tx_window_size = 8,
            .rx_window_size = 64,
            .tx_timeout = 15 * FBP_TIME_MILLISECOND,
            .tx_link_size = 128,
    };

    fbp_memset(&h_, 0, sizeof(h_));
    snprintf(device_path, sizeof(device_path), "%s", "COM3");  // default
    ARG_CONSUME(1);
    while (argc) {
        if ((argc >= 2) && (0 == strcmpi(argv[0], "--port"))) {
            snprintf(device_path, sizeof(device_path), "%s", argv[1]);
            ARG_CONSUME(2);
        } else if ((argc >= 2) && (0 == strcmpi(argv[0], "--baudrate"))) {
            if (fbp_cstr_to_u32(argv[1], &baudrate)) {
                printf("Invalid baudrate: %s\n", argv[1]);
                exit(1);
            }
            ARG_CONSUME(2);
        } else if ((argc >= 1) && (0 == strcmpi(argv[0], "--client"))) {
            pubsub_prefix[0] = 'c';
            port0_prefix[0] = 'c';
            port0_mode = FBP_PORT0_MODE_CLIENT;
            uint32_t window_sz = dl_config.tx_window_size;
            dl_config.tx_window_size = dl_config.rx_window_size;
            dl_config.rx_window_size = window_sz;
            ARG_CONSUME(1);
        } else {
            printf(USAGE);
            exit(1);
        }
    }

    h_.ctrl_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    FBP_ASSERT_ALLOC(h_.ctrl_event);
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        FBP_LOGW("Could not set control handler");
    }

    // printf("RAND_MAX = %ull\n", RAND_MAX);
    // srand(2);

    h_.pubsub = fbp_pubsub_initialize(pubsub_prefix, pubsub_buffer_size);
    if (!h_.pubsub) {
        FBP_LOGE("pubsub initialized failed");
        return 1;
    }

    fbp_topic_set(&topic, pubsub_prefix);
    fbp_topic_append(&topic, "en");
    fbp_pubsub_meta(h_.pubsub, topic.topic, enable_meta);
    fbp_pubsub_subscribe(h_.pubsub, topic.topic, FBP_PUBSUB_SFLAG_PUB, enable_sub, &h_);
    fbp_pubsub_publish(h_.pubsub, topic.topic, &fbp_union_u32_r(0), NULL, NULL);

    fbp_pubsub_subscribe(h_.pubsub, "", FBP_PUBSUB_SFLAG_PUB, pubsub_sub, &h_);

    struct fbp_uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_buffer_size = (FBP_FRAMER_MAX_SIZE + 4) / 2,
            .send_buffer_count = 8,
            .recv_buffer_size = FBP_FRAMER_MAX_SIZE,
            .recv_buffer_count = 16,
            .recv_fn = on_uart_recv,
            .recv_user_data = &h_,
    };

    fbp_os_mutex_t mutex;
    mutex = fbp_os_mutex_alloc("host");
    if (!mutex) {
        FBP_LOGE("fbp_os_mutex_alloc failed");
        return 1;
    }

    h_.uart = fbp_uart_alloc();
    if (!h_.uart) {
        FBP_LOGE("uart initialized failed");
        return 1;
    }

    if (fbp_uart_open(h_.uart, device_path, &uart_config)) {
        FBP_LOGE("fbp_uart_open failed");
        fbp_uart_free(h_.uart);
        return 1;
    }

    struct fbp_evm_s * evm = fbp_evm_allocate();
    // fbp_evm_register_schedule_callback(self->evm, on_schedule, self);
    fbp_evm_register_mutex(evm, mutex);
    struct fbp_evm_api_s evm_api;
    fbp_evm_api_get(evm, &evm_api);

    struct fbp_dl_ll_s ll = {
            .user_data = h_.uart,
            .send = (fbp_dl_ll_send_fn) fbp_uart_write,
            .send_available = (fbp_dl_ll_send_available_fn) fbp_uart_write_available,
    };

    h_.stack = fbp_stack_initialize(&dl_config, port0_mode, port0_prefix, &evm_api, &ll, h_.pubsub, NULL);
    if (!h_.stack) {
        FBP_LOGE("stack_initialize failed");
        return 1;
    }

    // https://www.windowscentral.com/how-manage-power-throttling-windows-10
    // https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/
    timeBeginPeriod(1);

    HANDLE handles[16];
    DWORD handle_count = 0;
    int64_t t_dl;
    int64_t t_evm;
    int64_t t_next;
    int64_t t_duration;
    DWORD duration_ms = 0;

    while (!h_.do_quit) {
        handle_count = FBP_ARRAY_SIZE(handles);
        fbp_uart_handles(h_.uart, &handle_count, handles);
        handles[handle_count++] = h_.ctrl_event;
        WaitForMultipleObjects(handle_count, handles, false, duration_ms);

        fbp_uart_process_write_completed(h_.uart);
        fbp_uart_process_read(h_.uart);
        fbp_pubsub_process(h_.pubsub);
        int64_t t_now = fbp_time_rel();
        t_dl = fbp_dl_process(h_.stack->dl, t_now);
        fbp_evm_process(evm, t_now);
        t_evm = fbp_evm_time_next(evm);
        fbp_uart_process_write_pend(h_.uart);

        t_next = fbp_time_max(t_now, fbp_time_min(t_dl, t_evm));
        t_duration = t_next - t_now;
        if (t_duration > FBP_TIME_SECOND) {
            duration_ms = 1000;
        } else {
            duration_ms = (DWORD) FBP_TIME_TO_COUNTER(t_next - t_now, 1000);
        }
        duration_ms = 1;
    }
    FBP_LOGI("shutting down by user request");
    timeEndPeriod(1);

    fbp_stack_finalize(h_.stack);
    fbp_uart_close(h_.uart);
    fbp_pubsub_finalize(h_.pubsub);
    CloseHandle(h_.ctrl_event);
    return 0;
}
