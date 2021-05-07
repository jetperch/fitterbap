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

/**
 * @file
 *
 * @brief Message ring buffer for variable-length messages.
 */

#ifndef FBP_COLLECTIONS_MESSAGE_RING_BUFFER_H__
#define FBP_COLLECTIONS_MESSAGE_RING_BUFFER_H__

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_collections
 * @defgroup fbp_collections_rbm Ring buffer for variable-length messages
 *
 * @brief Provide a simple ring buffer for first-in, first-out messages.
 *
 * The ring buffer is not thread-safe.  To use this buffer from
 * multiple threads, you must add a mutex.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The message ring buffer instance.
struct fbp_rbm_s {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    uint8_t * buf;
    uint32_t buf_size;  // Size of buf in bytes
};

/**
 * @brief Initialize the message ring buffer.
 *
 * @param self The ring buffer instance.
 * @param buffer The underlying memory to use for this buffer.
 * @param buffer_size The size of buffer in bytes.
 */
FBP_API void fbp_rbm_init(struct fbp_rbm_s * self, uint8_t * buffer, uint32_t buffer_size);

/**
 * @brief Clear all data from the memory buffer.
 *
 * @param self The ring buffer instance.
 */
FBP_API void fbp_rbm_clear(struct fbp_rbm_s * self);

/**
 * @brief Allocate a message on the ring buffer.
 *
 * @param self The ring buffer instance.
 * @param size The desired size of the buffer.
 * @return The buffer or NULL on out of space.
 *
 * The buffer is immediately allocated in the ring buffer.
 * The caller must populate the returned buffer before
 * relinquishing control, otherwise, the receiver will
 * get garbage data.  For multi-threaded applications,
 * wrap the allocation and buffer population with the
 * same mutex used by peek & pop.
 */
FBP_API uint8_t * fbp_rbm_alloc(struct fbp_rbm_s * self, uint32_t size);

/**
 * @brief Peek at the next message from the buffer.
 *
 * @param self The message ring buffer instance.
 * @param[out] size The size of buffer in bytes.
 * @return The buffer or NULL on empty.
 */
FBP_API uint8_t * fbp_rbm_peek(struct fbp_rbm_s * self, uint32_t * size);

/**
 * @brief Pop the next message from the buffer.
 *
 * @param self The message ring buffer instance.
 * @param[out] size The size of buffer in bytes.
 * @return The buffer or NULL on empty.
 */
FBP_API uint8_t * fbp_rbm_pop(struct fbp_rbm_s * self, uint32_t * size);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COLLECTIONS_MESSAGE_RING_BUFFER_H__ */
