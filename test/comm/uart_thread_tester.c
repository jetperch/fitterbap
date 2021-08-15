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
#include "fitterbap/cdef.h"
#include "fitterbap/cstr.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

// NOTE: reduce latency timer to 1 milliseconds for FTDI chips.
#define BUF32_LENGTH  (0x10000)         // must be power of 2, 2**16 or less
#define BUF8_LENGTH (4 * BUF32_LENGTH)
#define BUF8_MASK (BUF8_LENGTH - 1)

struct stats_s {
    uint64_t tx_count;
    uint64_t rx_count;
    int64_t time;
};

static uint32_t buf_u32[BUF32_LENGTH];
static uint8_t * buf_u8 = (uint8_t *) buf_u32;
static uint32_t tx_idx = 0;
static uint32_t rx_idx = 0;
static HANDLE ctrl_event;
static HANDLE rx_event;
struct fbp_uartt_s * uart = NULL;
volatile struct stats_s stats;
struct stats_s stats_prev;


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

static void uart_send() {
    uint32_t sz = fbp_uartt_send_available(uart);
    if ((tx_idx + sz) > BUF8_LENGTH) {
        uint32_t sz1 = BUF8_LENGTH - tx_idx;
        fbp_uartt_send(uart, buf_u8 + tx_idx, sz1);
        stats.tx_count += sz1;
        sz -= sz1;
        tx_idx = 0;
    }

    fbp_uartt_send(uart, buf_u8 + tx_idx, sz);
    stats.tx_count += sz;
    tx_idx = (tx_idx + sz) & BUF8_MASK;
}

static void on_uart_recv(void *user_data, uint8_t *buffer, uint32_t buffer_size) {
    stats.rx_count += buffer_size;
    // todo check data
    SetEvent(rx_event);
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    SetEvent(ctrl_event);
    return TRUE;
}

#define ARG_CONSUME(count)   argc -= count; argv += count

static const char USAGE[] =
        "usage: uart_thread_tester --port <port> --baudrate <baudrate>\n"
        "    port: The COM port path, such as COM3\n"
        "    baudrate: The baud rate\n"
        "\n";


int main(int argc, char * argv[]) {
    uint32_t baudrate = 3000000;
    char device_path[1024];
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
        } else {
            printf(USAGE);
            exit(1);
        }
    }

    ctrl_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    FBP_ASSERT_ALLOC(ctrl_event);
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        FBP_LOGW("Could not set control handler");
    }

    rx_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    FBP_ASSERT_ALLOC(rx_event);

    for (int i = 0; i < BUF32_LENGTH; ++i) {
        uint16_t v = (uint16_t) i;
        buf_u32[i] = (((uint32_t) v) << 16) | ~v;
    }

    fbp_allocator_set(hal_alloc, hal_free);
    fbp_log_initialize(app_log_printf_);

    struct uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_buffer_size = 512,
            .send_buffer_count = 8,
            .recv_buffer_size = 512,
            .recv_buffer_count = 8,
            .recv_fn = on_uart_recv,
            .recv_user_data = NULL,
    };

    uart = fbp_uartt_initialize(device_path, &uart_config);
    if (!uart) {
        FBP_LOGE("uart initialized failed");
        return 1;
    }

    if (fbp_uartt_start(uart)) {
        FBP_LOGE("fbp_uartt_start failed");
        return 1;
    }

    stats_prev.time = stats.time;
    uart_send();

    while (1) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(ctrl_event, 0)) {
            break;
        }
        if (WAIT_OBJECT_0 == WaitForSingleObject(rx_event, 10)) {
            ResetEvent(rx_event);
        }
        uart_send();
        stats.time = fbp_time_rel_ms();
        if ((stats.time - stats_prev.time) > 1000) {
            printf("tx=%I64d, rx=%I64d\n",
                   stats.tx_count - stats_prev.tx_count,
                   stats.rx_count - stats_prev.rx_count);
            stats_prev = stats;
        }
    }
    FBP_LOGI("shutting down by user request");

    fbp_uartt_finalize(uart);
    CloseHandle(rx_event);
    CloseHandle(ctrl_event);
    return 0;
}
