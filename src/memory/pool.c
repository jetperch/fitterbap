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

#include "fitterbap/memory/pool.h"
#include "fitterbap.h"
#include "utlist.h"

#define FBP_POOL_ALIGNMENT sizeof(int *)
#define MAGIC 0x9548CE12
#define STATUS_ALLOC 0xbfbf  /* bit[0] must be set! */

/**
 * @brief The object header in each pool element.
 */
struct fbp_pool_element_s {
    struct fbp_pool_element_s * next;
};

/**
 * @brief The memory pool instance.
 */
struct fbp_pool_s {
    /** A magic number used to verify that the pool pointer is valid. */
    uint32_t magic;
    /** The list of free elements. */
    struct fbp_pool_element_s * free_head;
};

struct fbp_pool_size_s {
    fbp_size_t block_hdr;
    fbp_size_t block_data;
    fbp_size_t pool_hdr;
    fbp_size_t element_sz;
    fbp_size_t sz;
};

static struct fbp_pool_size_s fbp_pool_size(int32_t block_count, int32_t block_size) {
    struct fbp_pool_size_s sz;
    sz.block_hdr = FBP_ROUND_UP_TO_MULTIPLE(sizeof(struct fbp_pool_element_s), FBP_POOL_ALIGNMENT);
    sz.block_data = FBP_ROUND_UP_TO_MULTIPLE(block_size, FBP_POOL_ALIGNMENT);
    sz.pool_hdr = FBP_ROUND_UP_TO_MULTIPLE(sizeof(struct fbp_pool_s), FBP_POOL_ALIGNMENT);
    sz.element_sz = sz.block_hdr + sz.block_data;
    sz.sz = sz.pool_hdr + sz.element_sz * block_count;
    return sz;
}

int32_t fbp_pool_instance_size(int32_t block_count, int32_t block_size) {
    struct fbp_pool_size_s sz = fbp_pool_size(block_count, block_size);
    return (int32_t) sz.sz;
}

int32_t fbp_pool_initialize(
        struct fbp_pool_s * self,
        int32_t block_count,
        int32_t block_size) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_GT_ZERO(block_count);
    FBP_DBC_GT_ZERO(block_size);
    struct fbp_pool_size_s sz = fbp_pool_size(block_count, block_size);
    fbp_memset(self, 0, sz.sz);
    self->magic = MAGIC;
    uint8_t * m = (uint8_t *) self;
    for (int32_t i = 0; i < block_count; ++i) {
        uint8_t * el = m + sz.pool_hdr + sz.element_sz * i + sz.block_hdr;
        el -= sizeof(struct fbp_pool_element_s);
        struct fbp_pool_element_s * hdr = (struct fbp_pool_element_s *) (el);
        LL_PREPEND(self->free_head, hdr);
    }

    return 0;
}

void fbp_pool_finalize(struct fbp_pool_s * self) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_EQUAL(self->magic, MAGIC);
    self->magic = 0;
}

int fbp_pool_is_empty(struct fbp_pool_s * self) {
    FBP_DBC_NOT_NULL(self);
    return (self->free_head ? 0 : 1);
}

void * fbp_pool_alloc(struct fbp_pool_s * self) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(self->free_head);
    struct fbp_pool_element_s * hdr = self->free_head;
    LL_DELETE(self->free_head, hdr);
    return ((void *) (hdr + 1));
}

void * fbp_pool_alloc_unsafe(struct fbp_pool_s * self) {
    FBP_DBC_NOT_NULL(self);
    if (!self->free_head) {
        return 0;
    }
    struct fbp_pool_element_s * hdr = self->free_head;
    LL_DELETE(self->free_head, hdr);
    return ((void *) (hdr + 1));
}

void fbp_pool_free(struct fbp_pool_s * self, void * block) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(block);
    struct fbp_pool_element_s * hdr = block;
    --hdr;
    LL_PREPEND(self->free_head, hdr);
}
