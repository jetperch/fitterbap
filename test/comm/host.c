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

#include "fitterbap/host/uart_thread.h"
#include "fitterbap/comm/stack.h"
#include "fitterbap/pubsub.h"

#include "fitterbap/collections/ring_buffer_u8.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/cstr.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/time.h"
// #include "fitterbap/host/uart.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

// NOTE: reduce latency timer to 1 milliseconds for FTDI chips.

struct host_s {
    // struct fbp_transport_s transport;
    struct fbp_uartt_s * uart;
    struct fbp_stack_s * stack;

    uint32_t tx_window_size;
    uint32_t time_last_ms;
    struct fbp_dl_status_s dl_status;
    struct fbp_pubsub_s * pubsub;

    HANDLE ctrl_event;
};

struct host_s h_;


void fbp_fatal(char const * file, int line, char const * msg) {
    printf("FATAL: %s:%d: %s\n", file, line, msg);
    fflush(stdout);
    exit(1);
}

static void * hal_alloc(fbp_size_t size_bytes) {
    return malloc((size_t) size_bytes);
}

static void hal_free(void * ptr) {
    free(ptr);
}

static void app_log_printf_(const char *format, ...) {
    va_list arg;
    printf("%d ", (uint32_t) fbp_time_rel_ms());
    va_start(arg, format);
    vprintf(format, arg);
    va_end(arg);
}

static void on_process(void * user_data) {
    struct host_s * self = (struct host_s *) user_data;
    fbp_stack_process(self->stack);
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    struct host_s * self = (struct host_s *) user_data;
    fbp_dl_ll_recv(self->stack->dl, buffer, buffer_size);
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    SetEvent(h_.ctrl_event);
    return TRUE;
}

#define ARG_CONSUME(count)   argc -= count; argv += count

static const char USAGE[] =
"usage: host.exe --port <port>\n"
"    port: The COM port path, such as COM3\n"
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

int main(int argc, char * argv[]) {
    uint32_t baudrate = 3000000;
    uint32_t pubsub_buffer_size = 1000000;
    char device_path[1024];
    fbp_memset(&h_, 0, sizeof(h_));
    snprintf(device_path, sizeof(device_path), "%s", "COM3");  // default
    ARG_CONSUME(1);
    while (argc) {
        if ((argc >= 2) && (0 == strcmpi(argv[0], "--port"))) {
            snprintf(device_path, sizeof(device_path), "%s", argv[1]);
            ARG_CONSUME(2);
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
    fbp_allocator_set(hal_alloc, hal_free);
    fbp_log_initialize(app_log_printf_);
    // srand(2);

    h_.pubsub = fbp_pubsub_initialize("h", pubsub_buffer_size);
    if (!h_.pubsub) {
        FBP_LOGE("pubsub initialized failed");
        return 1;
    }
    fbp_pubsub_subscribe(h_.pubsub, "", 0, pubsub_sub, &h_);

    struct uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_size_total = 3 * FBP_FRAMER_MAX_SIZE,
            .buffer_size = FBP_FRAMER_MAX_SIZE,
            .recv_buffer_count = 16,
            .recv_fn = on_uart_recv,
            .recv_user_data = &h_,
    };

    h_.uart = fbp_uartt_initialize(device_path, &uart_config);
    if (!h_.uart) {
        FBP_LOGE("uart initialized failed");
        return 1;
    }

    struct fbp_evm_api_s evm_api;
    fbp_uartt_evm_api(h_.uart, &evm_api);

    struct fbp_dl_config_s dl_config = {
            .tx_window_size = 8,
            .tx_buffer_size = 1 << 12,
            .rx_window_size = 64,
            .tx_timeout = 15 * FBP_TIME_MILLISECOND,
            .tx_link_size = 64,
    };

    struct fbp_dl_ll_s ll = {
            .user_data = h_.uart,
            .send = (fbp_dl_ll_send_fn) fbp_uartt_send,
            .send_available = (fbp_dl_ll_send_available_fn) fbp_uartt_send_available,
    };

    h_.stack = fbp_stack_initialize(&dl_config, FBP_PORT0_MODE_SERVER, "h/c0/", &evm_api, &ll, h_.pubsub);
    if (!h_.stack) {
        FBP_LOGE("stack_initialize failed");
        return 1;
    }

    fbp_os_mutex_t mutex;
    fbp_uartt_mutex(h_.uart, &mutex);
    fbp_dl_register_mutex(h_.stack->dl, mutex);

    if (fbp_uartt_start(h_.uart, on_process, &h_)) {
        FBP_LOGE("fbp_uartt_start failed");
        return 1;
    }

    while (1) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(h_.ctrl_event, 1)) {
            break;
        }
        fbp_pubsub_process(h_.pubsub);
    }
    FBP_LOGI("shutting down by user request");

    fbp_stack_finalize(h_.stack);
    fbp_uartt_finalize(h_.uart);
    fbp_pubsub_finalize(h_.pubsub);
    CloseHandle(h_.ctrl_event);
    return 0;
}
