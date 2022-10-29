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

#include "fitterbap/host/comm.h"
#include "fitterbap/cstr.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <windows.h>

// NOTE: reduce latency timer to 1 milliseconds for FTDI chips.

HANDLE ctrl_event;

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

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    (void) fdwCtrlType;
    SetEvent(ctrl_event);
    return TRUE;
}

#define ARG_CONSUME(count)   argc -= count; argv += count

static const char USAGE[] =
        "usage: comm.exe --port <port> --baudrate <baudrate>\n"
        "    port: The COM port path, such as COM3\n"
        "    baudrate: The baud rate\n"
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
    char device_path[1024];
    struct fbp_dl_config_s dl_config = {
            .tx_window_size = 8,
            .rx_window_size = 64,
            .tx_timeout = 15 * FBP_TIME_MILLISECOND,
            .tx_link_size = 128,
    };

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


    struct fbp_comm_s * c = fbp_comm_initialize(&dl_config, device_path, baudrate, pubsub_sub, NULL);
    if (!c) {
        FBP_LOGE("fbp_comm_initialize failed");
        return 1;
    }

    // https://www.windowscentral.com/how-manage-power-throttling-windows-10
    // https://randomascii.wordpress.com/2020/10/04/windows-timer-resolution-the-great-rule-change/
    timeBeginPeriod(1);
    WaitForSingleObject(ctrl_event, INFINITE);
    FBP_LOGI("shutting down by user request");
    fbp_comm_finalize(c);
    timeEndPeriod(1);
    CloseHandle(ctrl_event);
    return 0;
}
