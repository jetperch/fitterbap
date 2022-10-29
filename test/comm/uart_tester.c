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


struct stats_s {
    struct fbp_uart_status_s uart_status;
    uint64_t rx_byte_error_count;
    int64_t time;
};

struct state_s {
    uint16_t tx_lfsr;
    uint16_t rx_lfsr;

    struct fbp_uart_s * uart;
    struct stats_s stats;
    struct stats_s stats_prev;
};

static HANDLE ctrl_event;
static volatile bool quit_ = false;
static struct state_s state_;
static struct state_s * s = &state_;


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
    struct state_s * self = (struct state_s *) user_data;
    (void) buffer;

    uint16_t x = self->rx_lfsr;
    uint16_t b;

    for (uint32_t idx = 0; idx < buffer_size; ++idx) {
        if (buffer[idx] != (x & 0xffU)) {
            ++self->stats.rx_byte_error_count;
        }
        x = (x & 0xff00) | buffer[idx];
        b = ((x >> 0) ^ (x >> 2) ^ (x >> 3) ^ (x >> 5)) & 1;
        x = (x >> 1) | (b << 15);
    }
    self->rx_lfsr = x;
}

static void on_uart_send(void *user_data, uint8_t *buffer, uint32_t buffer_size, uint32_t remaining) {
    struct state_s * self = (struct state_s *) user_data;
    (void) self;
    (void) buffer;
    (void) buffer_size;
}

static void do_send(struct state_s * self) {
    uint8_t buffer[128];
    uint32_t sz = fbp_uart_write_available(self->uart);
    uint16_t x = self->tx_lfsr;
    uint16_t b;
    while (sz > sizeof(buffer)) {
        for (int i = 0; i < sizeof(buffer); ++i) {
            buffer[i] = (uint8_t) (x & 0xff);
            b = ((x >> 0) ^ (x >> 2) ^ (x >> 3) ^ (x >> 5)) & 1;
            x = (x >> 1) | (b << 15);
        }
        fbp_uart_write(self->uart, buffer, sizeof(buffer));
        sz -= sizeof(buffer);
    }
    self->tx_lfsr = x;
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    quit_ = true;
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

    struct fbp_uart_config_s uart_config = {
            .baudrate = baudrate,
            .send_buffer_size = 512,
            .send_buffer_count = 8,
            .recv_buffer_size = 512,
            .recv_buffer_count = 8,
            .recv_fn = on_uart_recv,
            .recv_user_data = s,
            .write_complete_fn = on_uart_send,
            .write_complete_user_data = s
    };

    s->uart = fbp_uart_alloc();
    if (!s->uart) {
        FBP_LOGE("uart initialized failed");
        return 1;
    }

    if (fbp_uart_open(s->uart, device_path, &uart_config)) {
        FBP_LOGE("uart fbp_uart_open failed");
        fbp_uart_free(s->uart);
        return 2;
    }

    s->tx_lfsr = 0xffffU;
    s->rx_lfsr = 0xffffU;
    s->stats.time = fbp_time_rel_ms();
    s->stats_prev.time = s->stats.time;
    HANDLE handles[16];
    DWORD handle_count = 0;
    int64_t iterations = 0;
    int64_t iterations_last = 0;

    while (!quit_) {
        handle_count = FBP_ARRAY_SIZE(handles);
        fbp_uart_handles(s->uart, &handle_count, handles);
        handles[handle_count++] = ctrl_event;
        WaitForMultipleObjects(handle_count, handles, false, 500);

        fbp_uart_process_write_completed(s->uart);
        fbp_uart_process_read(s->uart);
        do_send(s);
        fbp_uart_process_write_pend(s->uart);
        ++iterations;

        s->stats.time = fbp_time_rel_ms();
        if ((s->stats.time - s->stats_prev.time) > 1000) {
            fbp_uart_status_get(s->uart, &s->stats.uart_status);
            printf("tx=%I64d, rx=%I64d, dtx=%I64d, drx=%I64d, diter=%I64d, rx_byte_err=%I64d\n",
                   s->stats.uart_status.write_bytes,
                   s->stats.uart_status.read_bytes,
                   s->stats.uart_status.write_bytes - s->stats_prev.uart_status.write_bytes,
                   s->stats.uart_status.read_bytes - s->stats_prev.uart_status.read_bytes,
                   iterations - iterations_last,
                   s->stats.rx_byte_error_count);
            s->stats_prev = s->stats;
            iterations_last = iterations;
        }
    }
    FBP_LOGI("shutting down by user request");

    fbp_uart_close(s->uart);
    fbp_uart_free(s->uart);
    CloseHandle(ctrl_event);
    return 0;
}
