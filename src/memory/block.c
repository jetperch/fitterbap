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

#include "fitterbap/memory/block.h"
#include "fitterbap/cdef.h"
#include "fitterbap/dbc.h"

struct mblock_s {
    uint8_t * mem;
    int32_t mem_size;
    int32_t block_size;
    int32_t block_count;
    uint8_t bitmap[4]; // dynamically allocated to be the right size!
};

int32_t fbp_mblock_instance_size(int32_t mem_size, int32_t block_size) {
    FBP_DBC_GT_ZERO(mem_size);
    FBP_DBC_GT_ZERO(block_size);
    int32_t blocks_count = mem_size / block_size;
    int32_t bytes_count = (blocks_count + 7) / 8;
    int32_t bitmap_size = FBP_ROUND_UP_TO_MULTIPLE(bytes_count, 4);
    if (bitmap_size >= 4) {
        bitmap_size -= 4;
    }
    return (sizeof(struct mblock_s) + bitmap_size);
}

int32_t fbp_mblock_initialize(
        struct fbp_mblock_s * self,
        void * mem,
        int32_t mem_size,
        int32_t block_size) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(mem);
    FBP_DBC_GT_ZERO(mem_size);
    FBP_DBC_GT_ZERO(block_size);
    struct mblock_s * s = (struct mblock_s *) self;
    int32_t sz = fbp_mblock_instance_size(mem_size, block_size);
    fbp_memset(s, 0, sz);
    s->mem = (uint8_t *) mem;
    s->mem_size = mem_size;
    s->block_size = block_size;
    s->block_count = mem_size / block_size;
    return 0;
}

void fbp_mblock_finalize(struct fbp_mblock_s * self) {
    (void) self;
}

static inline int32_t size_to_blocks(struct mblock_s * s, int32_t size) {
    return (size + s->block_size - 1) / s->block_size;
}

void * fbp_mblock_alloc_unsafe(struct fbp_mblock_s * self, int32_t size) {
    // greedy allocator: take first space large enough
    struct mblock_s * s = (struct mblock_s *) self;
    FBP_DBC_NOT_NULL(s);
    FBP_DBC_GT_ZERO(size);
    int32_t blocks = size_to_blocks(s, size);
    int32_t idx_invalid = s->block_count + 1;
    int32_t idx_start = idx_invalid;
    int32_t free_count = 0;
    for (int32_t idx_search = 0; idx_search < s->block_count; ++idx_search) {
        int32_t bit = s->bitmap[idx_search / 8] >> (idx_search & 0x7);
        if (bit) {
            idx_start = idx_invalid;
            free_count = 0;
        } else if (idx_start == idx_invalid) {
            idx_start = idx_search;
            free_count = 1;
        } else {
            ++free_count;
        }
        if (free_count >= blocks) { // allocated, mark!
            for (int idx_alloc = idx_start; idx_alloc <= idx_search; ++idx_alloc) {
                s->bitmap[idx_alloc / 8] |= (uint8_t) (1 << (idx_alloc & 0x07));
            }
            return (s->mem + (idx_start * s->block_size));
        }
    }
    return 0;
}

void * fbp_mblock_alloc(struct fbp_mblock_s * self, int32_t size) {
    void * p = fbp_mblock_alloc_unsafe(self, size);
    FBP_ASSERT_ALLOC(p);
    return p;
}

void fbp_mblock_free(struct fbp_mblock_s * self, void * buffer, int32_t size) {
    struct mblock_s * s = (struct mblock_s *) self;
    uint8_t * b = (uint8_t *) buffer;
    FBP_ASSERT(b >= s->mem);
    FBP_ASSERT(b < (s->mem + s->mem_size));
    int32_t blocks = size_to_blocks(s, size);
    int32_t idx_start = ((int32_t) (b - s->mem)) / s->block_size;
    for (int idx = idx_start; idx < idx_start + blocks; ++idx) {
        int32_t bit = s->bitmap[idx / 8] >> (idx & 0x7);
        FBP_ASSERT(bit);  // ensure already allocated
        s->bitmap[idx / 8] &= ~((uint8_t) (1 << (idx & 0x07)));
    }
}
