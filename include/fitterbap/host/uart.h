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

/**
 * @file
 *
 * @brief UART abstraction.
 */

#ifndef FBP_HOST_UART_H_
#define FBP_HOST_UART_H_

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>

/**
* @ingroup fbp_host
* @defgroup fbp_host_uart Host UART interface
*
* @brief Host UART interface.
*
* @{
*/

FBP_CPP_GUARD_START

typedef void (*uart_recv_fn)(void *user_data, uint8_t *buffer, uint32_t buffer_size);

struct uart_config_s {
    uint32_t baudrate;
    uint32_t send_size_total;
    uint32_t buffer_size;
    uint32_t recv_buffer_count;
    uart_recv_fn recv_fn;
    void *recv_user_data;
};

struct uart_status_s {
    uint64_t write_bytes;
    uint64_t write_buffer_count;
    uint64_t read_bytes;
    uint64_t read_buffer_count;
};

struct uart_s;

FBP_API struct uart_s *uart_alloc();

FBP_API int32_t uart_open(struct uart_s *self, const char *device_path, struct uart_config_s const * config);

FBP_API void uart_close(struct uart_s *self);

FBP_API int32_t uart_write(struct uart_s *self, uint8_t const *buffer, uint32_t buffer_size);

FBP_API uint32_t uart_send_available(struct uart_s *self);

FBP_API void uart_process(struct uart_s *self);

FBP_API void uart_handles(struct uart_s *self, uint32_t * handle_count, void ** handles);

FBP_API int32_t uart_status_get(struct uart_s *self, struct uart_status_s * stats);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_HOST_UART_H_ */
