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

#include "fitterbap/memory/object_pool.h"
#include "fitterbap/argchk.h"
#include "fitterbap/assert.h"
#include "fitterbap/cdef.h"
#include "fitterbap/log.h"
#include "fitterbap.h"
#include "utlist.h"


#define FBP_POOL_ALIGNMENT 8  /* in total_bytes */
#define MAGIC 0x9548CE11
#define STATUS_ALLOC 0xbfbf  /* bit[0] must be set! */

/**
 * @brief The structure to store status when allocated.
 */
struct fbp_object_pool_alloc_s {
    uint16_t status;
    uint16_t count;
};

/**
 * @brief The object header in each pool element.
 */
struct fbp_object_pool_element_s {
    /** The pointer to the pool (used for deallocation). */
    struct fbp_object_pool_s * pool;
    union {
        /** The status when allocated. */
        struct fbp_object_pool_alloc_s alloc;
        /** The next free element when deallocated. */
        struct fbp_object_pool_element_s * next;
    } d;
};

FBP_STATIC_ASSERT((sizeof(intptr_t) != 4) ||
                           (8 == sizeof(struct fbp_object_pool_element_s)),
                   fbp_object_pool_element_header_size_32);
FBP_STATIC_ASSERT((sizeof(intptr_t) != 8) ||
                           (16 == sizeof(struct fbp_object_pool_element_s)),
                   fbp_object_pool_element_header_size_64);

/**
 * @brief The memory pool instance.
 */
struct fbp_object_pool_s {
    /** A magic number used to verify that the pool pointer is valid. */
    uint32_t magic;
    /** The list of free elements. */
    struct fbp_object_pool_element_s * free_head;
    int32_t obj_size;
    fbp_object_pool_constructor constructor;
    fbp_object_pool_destructor destructor;
};

struct fbp_object_pool_size_s {
    fbp_size_t obj_hdr;
    fbp_size_t obj_data;
    fbp_size_t pool_hdr;
    fbp_size_t element_sz;
    fbp_size_t sz;
};

static struct fbp_object_pool_size_s fbp_object_pool_size(int32_t obj_count, int32_t obj_size) {
    struct fbp_object_pool_size_s sz;
    sz.obj_hdr = FBP_ROUND_UP_TO_MULTIPLE(sizeof(struct fbp_object_pool_element_s), FBP_POOL_ALIGNMENT);
    sz.obj_data = FBP_ROUND_UP_TO_MULTIPLE(obj_size, FBP_POOL_ALIGNMENT);
    sz.pool_hdr = FBP_ROUND_UP_TO_MULTIPLE(sizeof(struct fbp_object_pool_s), FBP_POOL_ALIGNMENT);
    sz.element_sz = sz.obj_hdr + sz.obj_data;
    sz.sz = sz.pool_hdr + sz.element_sz * obj_count;
    return sz;
}

int32_t fbp_object_pool_instance_size(int32_t obj_count, int32_t obj_size) {
    struct fbp_object_pool_size_s sz = fbp_object_pool_size(obj_count, obj_size);
    return (int32_t) sz.sz;
}

int32_t fbp_object_pool_initialize(
        struct fbp_object_pool_s * self, int32_t obj_count, int32_t obj_size,
        fbp_object_pool_constructor constructor, fbp_object_pool_destructor destructor) {
    FBP_ARGCHK_NOT_NULL(self);
    FBP_ARGCHK_GT_ZERO(obj_count);
    FBP_ARGCHK_GT_ZERO(obj_size);
    struct fbp_object_pool_size_s sz = fbp_object_pool_size(obj_count, obj_size);
    fbp_memset(self, 0, sz.sz);
    self->magic = MAGIC;
    self->obj_size = obj_size;
    self->constructor = constructor;
    self->destructor = destructor;
    uint8_t * m = (uint8_t *) self;
    for (int32_t i = 0; i < obj_count; ++i) {
        uint8_t * el = m + sz.pool_hdr + sz.element_sz * i + sz.obj_hdr;
        el -= sizeof(struct fbp_object_pool_element_s);
        struct fbp_object_pool_element_s * hdr = (struct fbp_object_pool_element_s *) (el);
        hdr->pool = self;
        LL_PREPEND2(self->free_head, hdr, d.next);
    }
    return 0;
}

void fbp_object_pool_finalize(struct fbp_object_pool_s * self) {
    FBP_ASSERT(self);
    FBP_ASSERT(self->magic == MAGIC);
    self->magic = 0;
}

void * fbp_object_pool_alloc(struct fbp_object_pool_s * self) {
    struct fbp_object_pool_element_s * next = self->free_head;
    FBP_ASSERT(next);
    LL_DELETE2(self->free_head, next, d.next);
    next->d.alloc.status = STATUS_ALLOC;
    next->d.alloc.count = 1;
    ++next; // advance to object
    if (self->constructor) {
        self->constructor(next);
    } else {
        fbp_memset(next, 0, self->obj_size);
    }
    return next;
}

static inline struct fbp_object_pool_element_s * get_obj_header(void * obj) {
    uint8_t * m = (uint8_t *) obj;
    m -= sizeof(struct fbp_object_pool_element_s);
    struct fbp_object_pool_element_s * hdr = (struct fbp_object_pool_element_s *) m;
    FBP_ASSERT(hdr->d.alloc.status == STATUS_ALLOC);  // otherwise already free!
    return hdr;
}

void fbp_object_pool_incr(void * obj) {
    struct fbp_object_pool_element_s * hdr = get_obj_header(obj);
    FBP_ASSERT(hdr->d.alloc.count < 65535);
    ++hdr->d.alloc.count;
}

bool fbp_object_pool_decr(void * obj) {
    struct fbp_object_pool_element_s * hdr = get_obj_header(obj);
    if (hdr->d.alloc.count > 1) {
        --hdr->d.alloc.count;
        return false;
    } else if (hdr->d.alloc.count == 1) {
        if (hdr->pool->destructor) {
            hdr->pool->destructor(obj);
        }
        LL_PREPEND2(hdr->pool->free_head, hdr, d.next);
        return true;
    }
    FBP_FATAL("not allocated");
    return true;
}
