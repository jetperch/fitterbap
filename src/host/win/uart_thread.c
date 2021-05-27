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

#include "fitterbap/host/uart_thread.h"
#include "fitterbap/host/uart.h"
#include "fitterbap/event_manager.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/time.h"
#include <stdio.h>
#include <windows.h>


struct fbp_uartt_s {
    fbp_uartt_process_fn process_fn;
    void * process_user_data;
    struct fbp_evm_s * evm;
    fbp_os_mutex_t mutex;
    HANDLE thread;
    volatile int quit;
    struct uart_s * uart;
};

struct fbp_uartt_s * fbp_uartt_initialize(const char *device_path, struct uart_config_s const * config) {
    char dev_str[1024];
    struct fbp_uartt_s * self = fbp_alloc_clr(sizeof(struct fbp_uartt_s));
    if (!self) {
        return NULL;
    }

    FBP_LOGI("fbp_uartt_initialize(%s, %d)", device_path, (int) config->baudrate);
    self->mutex = fbp_os_mutex_alloc();

    self->evm = fbp_evm_allocate();
    fbp_evm_register_mutex(self->evm, self->mutex);
    struct fbp_evm_api_s evm_api;
    fbp_evm_api_get(self->evm, &evm_api);

    self->uart = uart_alloc();
    if (!self->uart) {
        FBP_LOGE("uart_alloc failed");
        fbp_uartt_finalize(self);
        return NULL;
    }

    snprintf(dev_str, sizeof(dev_str), "\\\\.\\%s", device_path);
    int32_t rv = uart_open(self->uart, dev_str, config);
    if (rv) {
        FBP_LOGE("uart_open failed with %d", (int) rv);
        fbp_uartt_finalize(self);
        return NULL;
    }

    return self;
}

void fbp_uartt_finalize(struct fbp_uartt_s *self) {
    if (self) {
        fbp_os_mutex_t mutex = self->mutex;
        if (mutex) {
            fbp_os_mutex_lock(mutex);
            self->mutex = NULL;
        }
        fbp_uartt_stop(self);
        if (self->uart) {
            uart_close(self->uart);
            fbp_free(self->uart);
            self->uart = NULL;
        }
        if (self->evm) {
            fbp_evm_free(self->evm);
            self->evm = NULL;
        }
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
            fbp_os_mutex_free(mutex);
        }
    }
}

static DWORD WINAPI task(LPVOID lpParam) {
    uint32_t handle_count;
    HANDLE handles[16];

    struct fbp_uartt_s * self = (struct fbp_uartt_s *) lpParam;
    while (!self->quit) {
        handle_count = 0;
        uart_handles(self->uart, &handle_count, &handles[handle_count]);
        int64_t time_start = fbp_time_rel();
        int64_t duration_i64 = fbp_evm_interval_next(self->evm, time_start);
        int64_t duration_ms = FBP_TIME_TO_COUNTER(duration_i64, 1000);
        if (duration_ms > 1) {
            duration_ms = 1;
        }

        if (handle_count) {
            WaitForMultipleObjects(handle_count, handles, FALSE, (DWORD) duration_ms);
        } else {
            Sleep((DWORD) duration_ms);
        }

        fbp_os_mutex_lock(self->mutex);
        uart_process(self->uart);
        fbp_os_mutex_unlock(self->mutex);
        fbp_evm_process(self->evm, fbp_time_rel());
        if (self->process_fn) {
            self->process_fn(self->process_user_data);
        }
    }
    return 0;
}

int32_t fbp_uartt_start(struct fbp_uartt_s * self,
                         fbp_uartt_process_fn process_fn, void * process_user_data) {
    self->process_fn = process_fn;
    self->process_user_data = process_user_data;
    if (self->thread) {
        FBP_LOGW("fbp_udl_start but already running");
        return FBP_ERROR_BUSY;
    }

    self->thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            task,                   // thread function name
            self,                   // argument to thread function
            0,                      // use default creation flags
            NULL);                  // returns the thread identifier
    if (!self->thread) {
        return FBP_ERROR_NOT_ENOUGH_MEMORY;
    }

    if (!SetThreadPriority(self->thread, THREAD_PRIORITY_TIME_CRITICAL)) {
        FBP_LOGE("Could not elevate thread priority: %d", (int) GetLastError());
    }
    return 0;
}

int32_t fbp_uartt_stop(struct fbp_uartt_s * self) {
    int rc = 0;
    if (self) {
        self->process_fn = NULL;
        self->quit = 1;
        if (self->thread) {
            if (WAIT_OBJECT_0 != WaitForSingleObject(self->thread, 1000)) {
                FBP_LOGW("UART thread failed to shut down gracefully");
                rc = FBP_ERROR_TIMED_OUT;
            }
            CloseHandle(self->thread);
            self->thread = NULL;
        }
    }
    return rc;
}

int32_t fbp_uartt_evm_api(struct fbp_uartt_s * self, struct fbp_evm_api_s * api) {
    return fbp_evm_api_get(self->evm, api);
}

void fbp_uartt_send(struct fbp_uartt_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    if (uart_send_available(self->uart) >= buffer_size) {
        uart_write(self->uart, buffer, buffer_size);
    }
}

uint32_t fbp_uartt_send_available(struct fbp_uartt_s * self) {
    return uart_send_available(self->uart);
}

void fbp_uartt_mutex(struct fbp_uartt_s * self, fbp_os_mutex_t * mutex) {
    *mutex = self->mutex;
}
