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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE

#include "fitterbap/host/uart.h"
#include "fitterbap/host/win/error.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/platform.h"
#include <windows.h>


struct buf_s {
    OVERLAPPED overlapped;
    uint32_t size;
    struct fbp_list_s item;
    uint8_t buf[];
};

struct fbp_uart_s {
    HANDLE handle;
    fbp_uart_recv_fn recv_fn;
    void * recv_user_data;
    fbp_uart_write_complete_fn write_complete_fn;
    void *write_complete_user_data;

    uint32_t write_buffer_size;
    struct fbp_list_s buf_write_issued;
    struct fbp_list_s buf_write_pend;
    struct fbp_list_s buf_write_free;

    uint32_t read_buffer_size;
    struct fbp_list_s buf_read;
    struct fbp_list_s buf_read_free;

    struct fbp_uart_status_s status;
};

static void read_pend_all(struct fbp_uart_s * self);

static struct buf_s * buf_alloc(uint32_t sz) {
    struct buf_s * buf = fbp_alloc_clr(sizeof(struct buf_s) + sz);
    FBP_ASSERT_ALLOC(buf);
    fbp_list_initialize(&buf->item);
    buf->overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    FBP_ASSERT_ALLOC(buf->overlapped.hEvent);
    return buf;
}

static void buf_free(struct buf_s * buf) {
    CloseHandle(buf->overlapped.hEvent);
    fbp_free(buf);
}

static inline struct buf_s * buf_alloc_from_list(struct fbp_list_s * list) {
    struct buf_s * buf;
    struct fbp_list_s * item = fbp_list_remove_head(list);
    if (!item) {
        return NULL;
    }
    buf = FBP_CONTAINER_OF(item, struct buf_s, item);
    buf->size = 0;
    buf->overlapped.Internal = 0;
    buf->overlapped.InternalHigh = 0;
    buf->overlapped.Offset = 0;
    buf->overlapped.OffsetHigh = 0;
    ResetEvent(buf->overlapped.hEvent);
    return buf;
}

static inline void buf_free_to_list(struct buf_s * buf, struct fbp_list_s * list) {
    FBP_ASSERT(buf);
    ResetEvent(buf->overlapped.hEvent);
    fbp_list_add_tail(list, &buf->item);
}

static struct buf_s * write_buf_alloc(struct fbp_uart_s * self) {
    struct buf_s * b = buf_alloc_from_list(&self->buf_write_free);
    return b;
}

static void write_buf_free(struct fbp_uart_s * self, struct buf_s * buf) {
    buf_free_to_list(buf, &self->buf_write_free);
}

static struct buf_s * read_buf_alloc(struct fbp_uart_s * self) {
    return buf_alloc_from_list(&self->buf_read_free);
}

static void read_buf_free(struct fbp_uart_s * self, struct buf_s * buf) {
    buf_free_to_list(buf, &self->buf_read_free);
}

struct fbp_uart_s * fbp_uart_alloc() {
    struct fbp_uart_s * self = (struct fbp_uart_s *) fbp_alloc(sizeof(struct fbp_uart_s));
    fbp_memset(self, 0, sizeof(self));
    self->handle = INVALID_HANDLE_VALUE;
    fbp_list_initialize(&self->buf_write_issued);
    fbp_list_initialize(&self->buf_write_pend);
    fbp_list_initialize(&self->buf_write_free);
    fbp_list_initialize(&self->buf_read);
    fbp_list_initialize(&self->buf_read_free);
    return self;
}

void fbp_uart_free(struct fbp_uart_s * self) {
    fbp_uart_close(self);
    fbp_free(self);
}

static void buf_list_free(struct fbp_list_s * list) {
    struct fbp_list_s * item;
    struct buf_s * buf;
    fbp_list_foreach(list, item) {
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        buf_free(buf);
    }
    fbp_list_initialize(list);
}

void fbp_uart_close(struct fbp_uart_s * self) {
    if (self->handle != INVALID_HANDLE_VALUE) {
        EscapeCommFunction(self->handle, CLRDTR);
        CloseHandle(self->handle);
        self->handle = INVALID_HANDLE_VALUE;
    }
    buf_list_free(&self->buf_write_issued);
    buf_list_free(&self->buf_write_free);
    buf_list_free(&self->buf_read);
    buf_list_free(&self->buf_read_free);
}

int32_t fbp_uart_open(struct fbp_uart_s * self, const char *device_path, struct fbp_uart_config_s const * config) {
    fbp_uart_close(self);
    self->recv_fn = config->recv_fn;
    self->recv_user_data = config->recv_user_data;
    self->write_complete_fn = config->write_complete_fn;
    self->write_complete_user_data = config->write_complete_user_data;
    self->write_buffer_size = config->send_buffer_size;
    self->read_buffer_size = config->recv_buffer_size;
    fbp_memset(&self->status, 0, sizeof(self->status));

    for (uint32_t i = 0; i < config->send_buffer_count; ++i) {
        fbp_list_add_tail(&self->buf_write_free, &buf_alloc(self->write_buffer_size)->item);
    }
    for (uint32_t i = 0; i < config->recv_buffer_count; ++i) {
        fbp_list_add_tail(&self->buf_read_free, &buf_alloc(self->read_buffer_size)->item);
    }

    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-dcb
    DCB dcb = {
            .DCBlength = sizeof(DCB),
            .BaudRate = config->baudrate,
            .fBinary = TRUE,
            .fParity = FALSE,
            .fOutxCtsFlow = FALSE,
            .fOutxDsrFlow = FALSE,
            .fDtrControl = DTR_CONTROL_DISABLE,
            .fDsrSensitivity = FALSE,
            .fTXContinueOnXoff = FALSE,
            .fOutX = FALSE,
            .fInX = FALSE,
            .fErrorChar = FALSE,
            .fNull = FALSE,
            .fRtsControl = RTS_CONTROL_DISABLE,
            .fAbortOnError = FALSE,
            .fDummy2 = 0,
            .wReserved = 0,
            .XonLim = 0,
            .XoffLim = 0,
            .ByteSize = 8,
            .Parity = NOPARITY,
            .StopBits = ONESTOPBIT,
            .XonChar = 0,
            .XoffChar = 0,
            .ErrorChar = 0,
            .EofChar = 0,
            .EvtChar = 0,
            .wReserved1 = 0,
    };

    // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts
    COMMTIMEOUTS timeouts = {
            .ReadIntervalTimeout = 1,
            .ReadTotalTimeoutMultiplier = 0,
            .ReadTotalTimeoutConstant = 16,
            .WriteTotalTimeoutMultiplier = 0,
            .WriteTotalTimeoutConstant = 100,
    };

    self->handle = CreateFileA(device_path,
                               GENERIC_READ | GENERIC_WRITE,
                               0,       // no file sharing
                               NULL,    // no security attributes
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                               NULL);
    if (self->handle == INVALID_HANDLE_VALUE) {
        return FBP_ERROR_NOT_FOUND;
    }
    if (!SetCommState(self->handle, &dcb)) {
        fbp_uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!SetCommTimeouts(self->handle, &timeouts)) {
        fbp_uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!FlushFileBuffers(self->handle)) {
        fbp_uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!PurgeComm(self->handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        fbp_uart_close(self);
        return FBP_ERROR_IO;
    }

    DWORD comm_errors = 0;
    ClearCommError(self->handle, &comm_errors, NULL);
    ClearCommBreak(self->handle);

    EscapeCommFunction(self->handle, SETDTR);
    read_pend_all(self);

    return 0;
}

FBP_API int32_t fbp_uart_write(struct fbp_uart_s *self, uint8_t const *buffer, uint32_t buffer_size) {
    uint32_t sz;
    uint32_t send_remaining = fbp_uart_write_available(self);
    struct fbp_list_s * item;
    struct buf_s * b = NULL;

    if (buffer_size > send_remaining) {
        FBP_LOGE("fbp_uart_write(%d bytes), but only %d remaining", (int) buffer_size, (int) send_remaining);
        return FBP_ERROR_NOT_ENOUGH_MEMORY;
    }
    FBP_LOGD3("fbp_uart_write(%d)", (int) buffer_size);

    item = fbp_list_peek_tail(&self->buf_write_pend);
    if (item) {
        b = FBP_CONTAINER_OF(item, struct buf_s, item);
    }

    while (buffer_size) {
        if (b && (b->size >= self->write_buffer_size)) {
            b = NULL;
        }
        if (!b) {
            b = write_buf_alloc(self);
            FBP_ASSERT_ALLOC(b);  // should never fail since we checked uart_send_available()
            fbp_list_add_tail(&self->buf_write_pend, &b->item);
        }

        sz = buffer_size;
        uint32_t remaining = self->write_buffer_size - b->size;
        if (sz > remaining) {
            sz = remaining;
        }
        fbp_memcpy(b->buf + b->size, buffer, sz);
        b->size += sz;
        buffer_size -= sz;
        buffer += sz;
    }
    return 0;
}

static int32_t read_pend_one(struct fbp_uart_s * self) {
    DWORD read_count = 0;
    struct buf_s * buf = read_buf_alloc(self);
    if (!buf) {
        return -1;
    }
    buf->size = self->read_buffer_size;

    if (ReadFile(self->handle, buf->buf, buf->size, &read_count, &buf->overlapped)) {
        // synchronous return, put into list to ensure in-order processing.
        FBP_LOGD1("read_pend sync: %d", (int) read_count);
        SetEvent(buf->overlapped.hEvent);
        buf->size = read_count;
        fbp_list_add_tail(&self->buf_read, &buf->item);
        return 1;
    } else if (GetLastError() == ERROR_IO_PENDING) {
        // normal, async return
        fbp_list_add_tail(&self->buf_read, &buf->item);
        return 0;
    } else {
        WINDOWS_LOGE("%s", "ReadFile not pending");
        read_buf_free(self, buf);
        return -1;
    }
}

static void read_pend_all(struct fbp_uart_s * self) {
    int32_t rc;
    while (1) {
        rc = read_pend_one(self);
        if (rc < 0) {
            return;
        }
    }
}

static void read_process_completed(struct fbp_uart_s *self) {
    struct fbp_list_s * item;
    struct buf_s * buf;
    while (!fbp_list_is_empty(&self->buf_read)) {
        DWORD read_count = 0;
        item = fbp_list_peek_head(&self->buf_read);
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        BOOL rc = GetOverlappedResult(self->handle, &buf->overlapped, &read_count, FALSE);
        DWORD last_error = rc ? NO_ERROR : GetLastError();
        if (last_error == ERROR_IO_INCOMPLETE) {
            break;  // still in progress
        }
        fbp_list_remove_head(&self->buf_read);
        if (rc) {
            if (read_count) {
                self->status.read_bytes += read_count;
                ++self->status.read_buffer_count;
                self->recv_fn(self->recv_user_data, buf->buf, read_count);
            }
        } else if (last_error == ERROR_TIMEOUT) {
            // no received data, no worries!
        } else {
            WINDOWS_LOGE("%s", "fbp_uart read error");
        }
        read_buf_free(self, buf);
        FBP_LOGD3("read %d", (int) read_count);
    }
}

void fbp_uart_process_read(struct fbp_uart_s *self) {
    read_process_completed(self);
    read_pend_all(self);
}

int32_t fbp_uart_process_write_pend(struct fbp_uart_s *self) {
    struct fbp_list_s * item;
    struct buf_s * buf;
    DWORD write_count;
    while (!fbp_list_is_empty(&self->buf_write_pend)) {
        item = fbp_list_remove_head(&self->buf_write_pend);
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        write_count = 0;
        if (WriteFile(self->handle, buf->buf, buf->size, &write_count, &buf->overlapped)) {
            // unusual synchronous completion, but ok
            FBP_LOGW("fbp_uart_write overlapped completed immediately");
            write_buf_free(self, buf);
        } else if (GetLastError() == ERROR_IO_PENDING) {
            // normal, expected overlapped IO path
            fbp_list_add_tail(&self->buf_write_issued, &buf->item);
            FBP_LOGD3("write pend: %d", (int) buf->size);
        } else {
            // overlapped error
            WINDOWS_LOGE("%s", "fbp_uart_write overlapped failed");
            write_buf_free(self, buf);
            return FBP_ERROR_IO;
        }
    }
    return 0;
}

void fbp_uart_process_write_completed(struct fbp_uart_s *self) {
    struct fbp_list_s * item;
    struct buf_s * buf;
    while (!fbp_list_is_empty(&self->buf_write_issued)) {
        DWORD bytes = 0;
        item = fbp_list_peek_head(&self->buf_write_issued);
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        BOOL rc = GetOverlappedResult(self->handle, &buf->overlapped, &bytes, FALSE);
        DWORD last_error = rc ? NO_ERROR : GetLastError();
        if (last_error == ERROR_IO_INCOMPLETE) {
            break;  // still in progress
        }
        self->status.write_bytes += buf->size;
        ++self->status.write_buffer_count;
        fbp_list_remove_head(&self->buf_write_issued);
        if (self->write_complete_fn) {
            self->write_complete_fn(self->write_complete_user_data, buf->buf, buf->size,
                                    fbp_list_length(&self->buf_write_issued));
        }
        write_buf_free(self, buf);
        FBP_LOGD3("write complete");
    }
}

uint32_t fbp_uart_write_available(struct fbp_uart_s *self) {
    struct fbp_list_s * item;
    struct buf_s * b;
    uint32_t sz = fbp_list_length(&self->buf_write_free) * self->write_buffer_size;
    item = fbp_list_peek_tail(&self->buf_write_pend);
    if (item) {
        b = FBP_CONTAINER_OF(item, struct buf_s, item);
        sz += self->write_buffer_size - b->size;
    }
    return sz;
}

void fbp_uart_handles(struct fbp_uart_s *self, uint32_t * handle_count, void ** handles) {
    DWORD count = 0;
    struct fbp_list_s * item;
    struct buf_s * buf;

    FBP_ASSERT(*handle_count >= 2);
    item = fbp_list_peek_head(&self->buf_write_issued);
    if (item) {
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        handles[count++] = buf->overlapped.hEvent;
    }

    item = fbp_list_peek_head(&self->buf_read);
    if (item) {
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        handles[count++] = buf->overlapped.hEvent;
    }
    *handle_count = count;
}

void fbp_uart_status_get(struct fbp_uart_s *self, struct fbp_uart_status_s * stats) {
    *stats = self->status;
}
