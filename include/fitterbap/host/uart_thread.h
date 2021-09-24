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

/**
 * @file
 *
 * @brief Threaded UART abstraction.
 */

#ifndef FBP_HOST_UART_THREAD_H_
#define FBP_HOST_UART_THREAD_H_

#include "fitterbap/host/uart.h"
#include "fitterbap/comm/data_link.h"
#include "fitterbap/os/mutex.h"
#include <stdint.h>

/**
 * @ingroup fbp_host
 * @defgroup fbp_host_uart_thread Threaded UART
 *
 * @brief Threaded UART.
 *
 * This UART thread implementation is compatible with the stream data_link
 * lower level API.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The opaque instance.
struct fbp_uartt_s;

FBP_API struct fbp_uartt_s * fbp_uartt_initialize(
        const char *device_path,
        struct uart_config_s const * config);

FBP_API void fbp_uartt_finalize(struct fbp_uartt_s *self);

FBP_API int32_t fbp_uartt_start(struct fbp_uartt_s * self);

FBP_API int32_t fbp_uartt_stop(struct fbp_uartt_s * self);

/**
 * @brief Populate the event manager API.
 *
 * @param self The UART thread instance.
 * @param api[out] The API instance populated with the event manager instance
 *      and callbacks running on the UART thread.
 * @return 0 or error code.
 *
 * All events for processing on this thread MUST be posted to this
 * event manager.
 */
FBP_API int32_t fbp_uartt_evm_api(struct fbp_uartt_s * self, struct fbp_evm_api_s * api);

/**
 * @brief Send data out the UART.
 *
 * @param self The UART thread instance.
 * @param buffer The buffer containing the data to send. The caller retains
 *      ownership, and the buffer is only valid for the duration of the call.
 * @param buffer_size The size of buffer in total_bytes.
 *
 * Compatible with fbp_dl_ll_send_fn.
 */
FBP_API void fbp_uartt_send(struct fbp_uartt_s * self, uint8_t const * buffer, uint32_t buffer_size);

/**
 * @brief The number of bytes currently available to send().
 *
 * @param self The UART thread instance.
 * @return The non-blocking free space available to send().
 *
 * Compatible with fbp_dl_ll_send_available_fn.
 */
FBP_API uint32_t fbp_uartt_send_available(struct fbp_uartt_s * self);

/**
 * @brief Get the mutex for accessing this thread.
 *
 * @param self The UART thread instance.
 * @param mutex[out] The mutex.
 */
FBP_API void fbp_uartt_mutex(struct fbp_uartt_s * self, fbp_os_mutex_t * mutex);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_HOST_UART_THREAD_H_ */
