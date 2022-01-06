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
 * An easy-to-use, high-performance UART implementation.
 * This module is not thread safe and is intended to be called
 * from within a dedicated UART thread.
 *
 * @{
*/

FBP_CPP_GUARD_START

/**
 * @brief The function called when UART receives data.
 *
 * @param user_data The arbitrary user data.
 * @param buffer The buffer containing the received data.
 * @param buffer_size The size of buffer in bytes.
 */
typedef void (*fbp_uart_recv_fn)(void *user_data, uint8_t *buffer, uint32_t buffer_size);

/**
 * @brief The function called when the UART completes sending data.
 *
 * @param user_data The arbitrary user data.
 * @param buffer The buffer containing the sent data.
 * @param buffer_size The size of buffer in bytes.
 * @param remaining The current number of remaining blocks to write.
 *
 * Note that fbp_uart_write() copies data into its internal buffer.
 * fbp_uart_write() may also split writes into multiple blocks or
 * aggregate multiple calls into a single underlying block.  This callback
 * is called once for each block, not once for each fbp_uart_write().
 */
typedef void (*fbp_uart_write_complete_fn)(void * user_data, uint8_t *buffer, uint32_t buffer_size, uint32_t remaining);

/**
 * @brief The UART configuration structure.
 */
struct fbp_uart_config_s {
    uint32_t baudrate;
    uint32_t send_buffer_size;
    uint32_t send_buffer_count;
    uint32_t recv_buffer_size;
    uint32_t recv_buffer_count;
    fbp_uart_recv_fn recv_fn;
    void *recv_user_data;
    fbp_uart_write_complete_fn write_complete_fn;
    void *write_complete_user_data;
};

/**
 * @brief The UART status structure.
 */
struct fbp_uart_status_s {
    uint64_t write_bytes;
    uint64_t write_buffer_count;
    uint64_t read_bytes;
    uint64_t read_buffer_count;
};

/// The opaque UART instance.
struct fbp_uart_s;

/**
 * @brief Allocate a new UART instance.
 *
 * @return The new UART instance.
 * @throw Assert on out of memory.
 * @see fbp_uart_free()
 */
FBP_API struct fbp_uart_s * fbp_uart_alloc();

/**
 * @brief Free a UART instance.
 *
 * @param self The UART instance.
 * @see fbp_uart_alloc()
 */
FBP_API void fbp_uart_free(struct fbp_uart_s * self);

/**
 * @brief Open a UART.
 *
 * @param self The UART instance created by  fbp_uart_alloc().
 * @param device_path The platform-specific device path string for the UART.
 * @param config The UART configuration.
 * @return 0 or error code.
 * @see fbp_uart_close().
 */
FBP_API int32_t fbp_uart_open(struct fbp_uart_s *self, const char *device_path, struct fbp_uart_config_s const * config);

/**
 * @brief Close a UART.
 *
 * @param self The UART instance.
 * @see fbp_uart_open()
 */
FBP_API void fbp_uart_close(struct fbp_uart_s *self);

/**
 * @brief Write (send) data out the UART.
 *
 * @param self The UART instance.
 * @param buffer The buffer containing the data to send.  This function copies
 *      to an internal buffer.  The caller retains ownership of buffer and
 *      may immediately modify the buffer contents.
 * @param buffer_size The size of buffer in bytes.
 * @return 0 or
 *      FBP_ERROR_NOT_ENOUGH_MEMORY if buffer_size > fbp_uart_write_available().
 *
 * This function queues write transactions.  Write transactions are not actually
 * posted to the operating system until the next call to
 * fbp_uart_process_write().
 */
FBP_API int32_t fbp_uart_write(struct fbp_uart_s *self, uint8_t const *buffer, uint32_t buffer_size);

/**
 * @brief Get the number of bytes currently available in the write buffer.
 *
 * @param self The UART instance.
 * @return The number of bytes currently available to write which
 *      may be filled using fbp_uart_write().
 */
FBP_API uint32_t fbp_uart_write_available(struct fbp_uart_s *self);

/**
 * @brief Process the UART receive data.
 *
 * @param self The UART instance.
 * @see fbp_uart_process_write()
 * @see fbp_uart_handles()
 *
 * This function must be called periodically from the UART thread, normally
 * after waiting on the UART handles.  This function is normally called
 * immediately at the start of the loop.
 */
FBP_API void fbp_uart_process_read(struct fbp_uart_s *self);

/**
 * @brief Process the completed UART send data.
 *
 * @param self The UART instance.
 * @see fbp_uart_process_read()
 * @see fbp_uart_process_write_pend()
 * @see fbp_uart_handles()
 *
 * This function must be called periodically from the UART thread, normally
 * after waiting on the UART handles.  This function is normally called
 * immediately after fbp_uart_process_read() before the loop starts any
 * application-specific processing.  Call fbp_uart_process_write_pend()
 * after the application-specific processing.
 */
FBP_API void fbp_uart_process_write_completed(struct fbp_uart_s *self);

/**
 * @brief Process the pending UART send data.
 *
 * @param self The UART instance.
 * @return 0 or error code.
 * @see fbp_uart_process_read()
 * @see fbp_uart_process_write_completed()
 * @see fbp_uart_handles()
 *
 * This function must be called periodically from the UART thread, normally
 * after waiting on the UART handles.  This function is normally called at
 * the end of the loop after fbp_uart_process_write_completed() and
 * any application-specific processing.
 */
FBP_API int32_t fbp_uart_process_write_pend(struct fbp_uart_s *self);

/**
 * @brief Get the UART filehandles for select / WaitForMultipleObjects.
 *
 * @param self The UART instance.
 * @param handle_count[inout] On input, the size of handles in elements.
 *      On output, the number of elements set in handles.
 * @param handles[out] The handles for this UART instance.
 */
FBP_API void fbp_uart_handles(struct fbp_uart_s *self, uint32_t * handle_count, void ** handles);

/**
 * @brief Get the current UART status.
 *
 * @param self The UART instance.
 * @param stats[out] The current status.
 */
FBP_API void fbp_uart_status_get(struct fbp_uart_s *self, struct fbp_uart_status_s * stats);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_HOST_UART_H_ */
