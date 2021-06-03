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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_DEBUG1

#include "fitterbap/host/uart.h"
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

struct uart_s {
    HANDLE handle;
    uart_recv_fn recv_fn;
    void * recv_user_data;

    uint32_t write_buffer_size;
    struct fbp_list_s buf_write;
    struct fbp_list_s buf_write_free;

    uint32_t read_buffer_size;
    struct fbp_list_s buf_read;
    struct fbp_list_s buf_read_free;

    struct uart_status_s status;
};

// https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
// This functions fills a caller-defined character buffer (pBuffer)
// of max length (cchBufferLength) with the human-readable error message
// for a Win32 error code (dwErrorCode).
//
// Returns TRUE if successful, or FALSE otherwise.
// If successful, pBuffer is guaranteed to be NUL-terminated.
// On failure, the contents of pBuffer are undefined.
BOOL GetErrorMessage(DWORD dwErrorCode, char * pBuffer, DWORD cchBufferLength) {
    char* p = pBuffer;
    if (cchBufferLength == 0) {
        return FALSE;
    }
    pBuffer[0] = 0;

    DWORD cchMsg = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
                                  dwErrorCode,
                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  pBuffer,
                                  cchBufferLength,
                                  NULL);

    while (*p) {
        if ((*p == '\n') || (*p == '\r')) {
            *p = 0;
            break;
        }
        ++p;
    }
    return (cchMsg > 0);
}

#define WINDOWS_LOGE(format, ...) { \
    char error_msg_[64]; \
    DWORD error_ = GetLastError(); \
    GetErrorMessage(error_, error_msg_, sizeof(error_msg_)); \
    FBP_LOGE(format ": %d: %s", __VA_ARGS__, (int) error_, error_msg_); \
}

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

static void buf_reset(struct buf_s * buf) {
    buf->overlapped.Internal = 0;
    buf->overlapped.InternalHigh = 0;
    buf->overlapped.Offset = 0;
    buf->overlapped.OffsetHigh = 0;
    ResetEvent(buf->overlapped.hEvent);
}

static inline struct buf_s * buf_alloc_from_list(struct fbp_list_s * list) {
    struct buf_s * buf;
    struct fbp_list_s * item = fbp_list_remove_head(list);
    if (!item) {
        return NULL;
    }
    buf = FBP_CONTAINER_OF(item, struct buf_s, item);
    return buf;
}

static inline void buf_free_to_list(struct buf_s * buf, struct fbp_list_s * list) {
    if (buf) {
        ResetEvent(buf->overlapped.hEvent);
        fbp_list_add_tail(list, &buf->item);
    }
}

static struct buf_s * write_buf_alloc(struct uart_s * self) {
    return buf_alloc_from_list(&self->buf_write_free);
}

static void write_buf_free(struct uart_s * self, struct buf_s * buf) {
    buf_free_to_list(buf, &self->buf_write_free);
}

static struct buf_s * read_buf_alloc(struct uart_s * self) {
    return buf_alloc_from_list(&self->buf_read_free);
}

static void read_buf_free(struct uart_s * self, struct buf_s * buf) {
    buf_free_to_list(buf, &self->buf_read_free);
}

struct uart_s * uart_alloc() {
    struct uart_s * self = (struct uart_s *) fbp_alloc(sizeof(struct uart_s));
    fbp_memset(self, 0, sizeof(self));
    self->handle = INVALID_HANDLE_VALUE;
    fbp_list_initialize(&self->buf_write);
    fbp_list_initialize(&self->buf_write_free);
    fbp_list_initialize(&self->buf_read);
    fbp_list_initialize(&self->buf_read_free);
    return self;
}

void uart_free(struct uart_s * self) {
    uart_close(self);
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

void uart_close(struct uart_s * self) {
    if (self->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(self->handle);
        self->handle = INVALID_HANDLE_VALUE;
    }
    buf_list_free(&self->buf_write);
    buf_list_free(&self->buf_write_free);
    buf_list_free(&self->buf_read);
    buf_list_free(&self->buf_read_free);
}

int32_t uart_open(struct uart_s * self, const char *device_path, struct uart_config_s const * config) {
    uart_close(self);
    self->recv_fn = config->recv_fn;
    self->recv_user_data = config->recv_user_data;
    self->write_buffer_size = config->send_buffer_size;
    self->read_buffer_size = config->recv_buffer_size;

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
            .ReadIntervalTimeout = 4,
            .ReadTotalTimeoutMultiplier = 0,
            .ReadTotalTimeoutConstant = 8,
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
        uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!SetCommTimeouts(self->handle, &timeouts)) {
        uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!FlushFileBuffers(self->handle)) {
        uart_close(self);
        return FBP_ERROR_IO;
    }
    if (!PurgeComm(self->handle, PURGE_RXCLEAR | PURGE_TXCLEAR)) {
        uart_close(self);
        return FBP_ERROR_IO;
    }

    DWORD comm_errors = 0;
    ClearCommError(self->handle, &comm_errors, NULL);
    ClearCommBreak(self->handle);

    return 0;
}

int32_t uart_write(struct uart_s * self, uint8_t const * buffer, uint32_t buffer_size) {
    uint32_t sz;
    struct buf_s * buf;
    uint32_t send_remaining = uart_send_available(self);
    if (buffer_size > send_remaining) {
        FBP_LOGE("uart_write(%d bytes), but only %d remaining", (int) buffer_size, (int) send_remaining);
        return FBP_ERROR_NOT_ENOUGH_MEMORY;
    }
    while (buffer_size) {
        sz = buffer_size;
        if (sz > self->write_buffer_size) {
            sz = self->write_buffer_size;
        }
        DWORD write_count = 0;
        buf = write_buf_alloc(self);
        FBP_ASSERT_ALLOC(buf);  // should never fail since we checked uart_send_available()
        fbp_memcpy(buf->buf, buffer, sz);

        if (WriteFile(self->handle, buf->buf, sz, &write_count, &buf->overlapped)) {
            // unusual synchronous completion, but ok
            FBP_LOGW("uart_write overlapped completed immediately");
            buffer += write_count;
            buffer_size -= write_count;
            write_buf_free(self, buf);
        } else if (GetLastError() == ERROR_IO_PENDING) {
            // normal, expected overlapped IO path
            buffer += sz;
            buffer_size -= sz;
            fbp_list_add_tail(&self->buf_write, &buf->item);
            FBP_LOGD3("write pend: %d", (int) sz);
        } else {
            // overlapped error
            WINDOWS_LOGE("%s", "uart_write overlapped failed");
            write_buf_free(self, buf);
            return FBP_ERROR_IO;
        }
    }

    return 0;
}

uint32_t uart_send_available(struct uart_s *self) {
    return fbp_list_length(&self->buf_write_free) * self->write_buffer_size;
}

static int32_t read_pend_one(struct uart_s * self) {
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

static void read_pend_all(struct uart_s * self) {
    int32_t rc;
    while (1) {
        rc = read_pend_one(self);
        if (rc < 0) {
            return;
        }

    }
}

static void process_read(struct uart_s *self) {
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
            WINDOWS_LOGE("%s", "process_read error");
        }
        read_buf_free(self, buf);
        read_pend_all(self);
        FBP_LOGD3("read %d", (int) read_count);
    }

    read_pend_all(self);
}

static void process_write(struct uart_s *self) {
    struct fbp_list_s * item;
    struct buf_s * buf;
    while (!fbp_list_is_empty(&self->buf_write)) {
        DWORD bytes = 0;
        item = fbp_list_peek_head(&self->buf_write);
        buf = FBP_CONTAINER_OF(item, struct buf_s, item);
        BOOL rc = GetOverlappedResult(self->handle, &buf->overlapped, &bytes, FALSE);
        DWORD last_error = rc ? NO_ERROR : GetLastError();
        if (last_error == ERROR_IO_INCOMPLETE) {
            break;  // still in progress
        }
        self->status.write_bytes += buf->size;
        ++self->status.write_buffer_count;
        fbp_list_remove_head(&self->buf_write);
        write_buf_free(self, buf);
        FBP_LOGD3("write complete");
    }
}

void uart_handles(struct uart_s *self, uint32_t * handle_count, void ** handles) {
    DWORD count = 0;
    struct fbp_list_s * item;
    struct buf_s * buf;

    item = fbp_list_peek_head(&self->buf_write);
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

void uart_process(struct uart_s *self) {
    process_read(self);
    process_write(self);
}

int32_t uart_status_get(struct uart_s *self, struct uart_status_s * stats) {
    *stats = self->status;
    return 0;
}
