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
 * @brief A memory pool for fixed-size blocks.
 */

#ifndef FBP_MEMORY_POOL_H_
#define FBP_MEMORY_POOL_H_

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_memory
 * @defgroup fbp_memory_pool Memory pool for fixed-size blocks
 *
 * @brief A memory pool for fixed-size blocks.
 *
 * This memory pool implementation provides constant time allocation
 * and constant time deallocation with no risk of fragmentation.
 *
 * This implementation is not thread safe.
 *
 * References include:
 *
 * - https://en.wikipedia.org/wiki/Reference_counting
 * - https://en.wikipedia.org/wiki/Memory_pool
 * - http://www.barrgroup.com/Embedded-Systems/How-To/Malloc-Free-Dynamic-Memory-Allocation
 * - http://nullprogram.com/blog/2015/02/17/
 * - http://nullprogram.com/blog/2014/10/21/
 *
 * @{
 */

FBP_CPP_GUARD_START

// Forward declaration for internal structure.
struct fbp_pool_s;

/**
 * @brief Get the pool instance size.
 *
 * @param block_count The number of blocks in the pool.
 * @param block_size The size of each block in total_bytes.
 * @return The required size for fbp_pool_s in total_bytes.
 */
FBP_API int32_t fbp_pool_instance_size(int32_t block_count,
                                         int32_t block_size);

/**
 * @brief Initialize a new memory pool.
 *
 * @param[out] self The memory pool to initialize which must be at least
 *      fbp_pool_instance_size(block_count, block_size) total_bytes.
 * @param block_count The number of blocks in the pool.
 * @param block_size The size of each block in total_bytes.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_pool_initialize(
        struct fbp_pool_s * self,
        int32_t block_count,
        int32_t block_size);

/**
 * @brief Finalize the memory pool instance.
 *
 * @param self The memory pool initialize by fbp_pool_initialize().
 *
 * This function does not free the instance memory as the allocated memory was
 * provided to fbp_pool_initialize().
 */
FBP_API void fbp_pool_finalize(struct fbp_pool_s * self);

/**
 * @brief Check if all blocks are allocated from the pool.
 *
 * @param self The memory pool instance.
 * @return 1 if empty, 0 if more blocks are available.
 */
FBP_API int fbp_pool_is_empty(struct fbp_pool_s * self);

/**
 * @brief Allocate a new block from the pool.
 *
 * @param self The memory pool instance.
 * @return The new block from the pool.
 *
 * This function will ASSERT and not return on out of memory conditions.
 */
FBP_API void * fbp_pool_alloc(struct fbp_pool_s * self);

/**
 * @brief Allocate a new block from the pool.
 *
 * @param self The memory pool instance.
 * @return The new block from the pool or 0.
 */
FBP_API void * fbp_pool_alloc_unsafe(struct fbp_pool_s * self);

/**
 * @brief Free a block previous allocated from the pool.
 *
 * @param self The memory pool instance.
 * @param block The block returned by fbp_pool_alloc().
 */
FBP_API void fbp_pool_free(struct fbp_pool_s * self, void * block);

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_MEMORY_POOL_H_ */
