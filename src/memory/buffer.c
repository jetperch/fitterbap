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

//#define FBP_LOG_LEVEL FBP_LOG_LEVEL_ALL
#include "fitterbap/memory/buffer.h"
#include "fitterbap/dbc.h"
#include "fitterbap/platform.h"
#include "fitterbap/memory/bbuf.h"

struct fbp_buffer_allocator_s {
    fbp_size_t size_max;
};

struct pool_s {
    const struct fbp_buffer_manager_s manager;
    uint32_t magic;
    fbp_size_t payload_size;
    fbp_size_t alloc_current;
    fbp_size_t alloc_max;
    uint8_t * memory_start;
    uint8_t * memory_end;
    uint16_t memory_incr;
    struct fbp_list_s buffers;
};

// Calculate the size to allocate for each structure, respecting platform
// alignment concerns
#define ALIGN (8)
#define MGR_SZ FBP_ROUND_UP_TO_MULTIPLE((fbp_size_t) sizeof(struct fbp_buffer_allocator_s), ALIGN)
#define POOL_SZ FBP_ROUND_UP_TO_MULTIPLE((fbp_size_t) sizeof(struct pool_s), ALIGN)
#define HDR_SZ FBP_ROUND_UP_TO_MULTIPLE((fbp_size_t) sizeof(struct fbp_buffer_s), ALIGN)
FBP_STATIC_ASSERT((sizeof(intptr_t) == 4) ? (32 == HDR_SZ) : (48 == HDR_SZ), header_size);
#define FBP_BUFFER_MAGIC (0xb8392f19)


static void fbp_buffer_free_(struct fbp_buffer_manager_s const * self, struct fbp_buffer_s * buffer);

static inline struct pool_s * pool_get(struct fbp_buffer_allocator_s * self, fbp_size_t index) {
    return (struct pool_s *) (((uint8_t *) self) + MGR_SZ + (POOL_SZ * index));
}

static inline void buffer_init(struct fbp_buffer_s * b) {
    b->cursor = 0;
    b->length = 0;
    b->reserve = 0;
    b->buffer_id = 0;
    b->flags = 0;
}

fbp_size_t fbp_buffer_allocator_instance_size(
        fbp_size_t const * sizes, fbp_size_t length) {
    FBP_DBC_NOT_NULL(sizes);
    fbp_size_t total_size = 0;
    total_size = MGR_SZ + POOL_SZ * length;
    for (fbp_size_t i = 0; i < length; ++i) {
        fbp_size_t buffer_sz = (32 << i);
        total_size += sizes[i] * (HDR_SZ + buffer_sz);
    }
    return total_size;
}

void fbp_buffer_allocator_initialize(
        struct fbp_buffer_allocator_s * self,
        fbp_size_t const * sizes, fbp_size_t length) {
    FBP_DBC_NOT_NULL(sizes);
    fbp_size_t total_size = 0;
    total_size = MGR_SZ + POOL_SZ * length;
    fbp_size_t header_size = total_size;
    for (fbp_size_t i = 0; i < length; ++i) {
        fbp_size_t buffer_sz = (32 << i);
        total_size += sizes[i] * (HDR_SZ + buffer_sz);
    }
    fbp_memset(self, 0, total_size);
    uint8_t * memory = (uint8_t *) self;
    self->size_max = total_size; // largest buffer
    uint8_t * buffers_ptr = memory + header_size;

    for (fbp_size_t i = 0; i < length; ++i) {
        struct pool_s * pool = pool_get(self, i);
        struct fbp_buffer_manager_s * m = (struct fbp_buffer_manager_s *) &pool->manager;
        m->free = fbp_buffer_free_;
        pool->magic = FBP_BUFFER_MAGIC;
        pool->payload_size = (32 << i);
        pool->alloc_current = 0;
        pool->alloc_max = 0;
        fbp_list_initialize(&pool->buffers);
        pool->memory_start = buffers_ptr;
        pool->memory_incr = (uint16_t) pool->payload_size + HDR_SZ;
        for (fbp_size_t k = 0; k < sizes[i]; ++k) {
            struct fbp_buffer_s * b = (struct fbp_buffer_s *) buffers_ptr;
            struct fbp_buffer_manager_s ** m_ptr = (struct fbp_buffer_manager_s **) &b->manager;
            *m_ptr = m;
            uint8_t ** d = (uint8_t **) &b->data; // discard const
            *d = buffers_ptr + HDR_SZ;
            uint16_t * capacity = (uint16_t *) &b->capacity;  // discard const
            FBP_ASSERT(pool->payload_size <= 0xffff);
            *capacity = (uint16_t) pool->payload_size;
            buffer_init(b);
            fbp_list_initialize(&b->item);
            fbp_list_add_tail(&pool->buffers, &b->item);
            buffers_ptr += pool->memory_incr;
        }
        pool->memory_end = buffers_ptr;
    }
    FBP_ASSERT(buffers_ptr == (memory + total_size));
}

FBP_API struct fbp_buffer_allocator_s * fbp_buffer_allocator_new(
        fbp_size_t const * sizes, fbp_size_t length) {
    fbp_size_t sz = fbp_buffer_allocator_instance_size(sizes, length);
    struct fbp_buffer_allocator_s * s = fbp_alloc(sz);
    fbp_buffer_allocator_initialize(s, sizes, length);
    return s;
}

void fbp_buffer_allocator_finalize(struct fbp_buffer_allocator_s * self) {
    (void) self;
    // audit outstanding buffers?
}

static fbp_size_t size_to_index_(struct fbp_buffer_allocator_s * self, fbp_size_t size) {
    FBP_ASSERT((size > 0) && (size <= self->size_max));
    fbp_size_t index = 32 - fbp_clz((uint32_t) (size - 1));
    if (index < 5) {
        index = 0; // 32 total_bytes is smallest
    } else {
        index = index - 5;
    }
    return index;
}

static inline struct fbp_buffer_s * alloc_(
        struct fbp_buffer_allocator_s * self, fbp_size_t size) {
    fbp_size_t index = size_to_index_(self, size);
    struct pool_s * p = pool_get(self, index);
    struct fbp_list_s * item = fbp_list_remove_head(&p->buffers);
    if (0 == item) {
        return 0;
    }
    struct fbp_buffer_s * buffer = fbp_list_entry(item, struct fbp_buffer_s, item);
    ++p->alloc_current;
    if (p->alloc_current > p->alloc_max) {
        p->alloc_max = p->alloc_current;
    }
    buffer_init(buffer);
    return buffer;
}


struct fbp_buffer_s * fbp_buffer_alloc(
        struct fbp_buffer_allocator_s * self, fbp_size_t size) {
    struct fbp_buffer_s * b = alloc_(self, size);
    FBP_ASSERT_ALLOC(b);
    FBP_LOGD3("fbp_buffer_alloc %p", (void *) b);
    return b;
}

struct fbp_buffer_s * fbp_buffer_alloc_unsafe(
        struct fbp_buffer_allocator_s * self,
        fbp_size_t size) {
    struct fbp_buffer_s * b = alloc_(self, size);
    FBP_LOGD3("fbp_buffer_alloc_unsafe %p", (void *) b);
    return b;
}

static void fbp_buffer_free_(struct fbp_buffer_manager_s const * self, struct fbp_buffer_s * buffer) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(buffer);
    FBP_LOGD3("fbp_buffer_free_(%p, %p)", (void *) self, (void *) buffer);
    struct pool_s * p = FBP_CONTAINER_OF(self, struct pool_s, manager);
    FBP_ASSERT(p->magic == FBP_BUFFER_MAGIC);
    uint8_t * b_ptr = (uint8_t *) buffer;
    FBP_ASSERT((b_ptr >= p->memory_start) && (b_ptr < p->memory_end));
    fbp_list_add_tail(&p->buffers, &buffer->item);
    --p->alloc_current;
}

static inline void write_update_length(struct fbp_buffer_s * buffer) {
    if (buffer->cursor > buffer->length) {
        buffer->length = buffer->cursor;
    }
}

void fbp_buffer_write(struct fbp_buffer_s * self,
                       void const * data,
                       fbp_size_t size) {
    FBP_DBC_NOT_NULL(self);
    if (size > 0) {
        FBP_DBC_NOT_NULL(data);
        FBP_ASSERT(size <= fbp_buffer_write_remaining(self));
        uint8_t * ptr = self->data + self->cursor;
        fbp_memcpy(ptr, data, size);
        self->cursor += (uint16_t) size;
        write_update_length(self);
    }
}

void fbp_buffer_copy(struct fbp_buffer_s * destination,
                      struct fbp_buffer_s * source,
                      fbp_size_t size) {
    FBP_DBC_NOT_NULL(destination);
    FBP_DBC_NOT_NULL(source);
    FBP_ASSERT(size <= fbp_buffer_read_remaining(source));
    FBP_ASSERT(size <= fbp_buffer_write_remaining(destination));
    if (size > 0) {
        uint8_t *dst = destination->data + destination->cursor;
        uint8_t *src = source->data + source->cursor;
        fbp_memcpy(dst, src, size);
        destination->cursor += (uint16_t) size;
        write_update_length(destination);
    }
}

static inline bool write_str_(struct fbp_buffer_s * self,
                              char const * str) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(str);
    uint16_t capacity = self->capacity - self->reserve;
    while (self->cursor < capacity) {
        if (*str == 0) {
            write_update_length(self);
            return true;
        }
        self->data[self->cursor] = *str;
        ++str;
        ++self->cursor;
    }
    self->length = capacity;
    return false;
}

void fbp_buffer_write_str(struct fbp_buffer_s * self,
                           char const * str) {
    FBP_ASSERT(write_str_(self, str));
}

bool fbp_buffer_write_str_truncate(struct fbp_buffer_s * self,
                                    char const * str) {
    return write_str_(self, str);
}


#define WRITE(buffer, value, buftype) \
    FBP_DBC_NOT_NULL(buffer); \
    FBP_ASSERT((fbp_size_t) sizeof(value) <= fbp_buffer_write_remaining(buffer)); \
    uint8_t * ptr = buffer->data + buffer->cursor; \
    FBP_BBUF_ENCODE_##buftype (ptr, value); \
    buffer->cursor += sizeof(value); \
    write_update_length(buffer);

void fbp_buffer_write_u8(struct fbp_buffer_s * self, uint8_t value) {
    WRITE(self, value, U8);
}

void fbp_buffer_write_u16_le(struct fbp_buffer_s * self, uint16_t value) {
    WRITE(self, value, U16_LE);
}

void fbp_buffer_write_u32_le(struct fbp_buffer_s * self, uint32_t value) {
    WRITE(self, value, U32_LE);
}

void fbp_buffer_write_u64_le(struct fbp_buffer_s * self, uint64_t value) {
    WRITE(self, value, U64_LE);
}

void fbp_buffer_write_u16_be(struct fbp_buffer_s * self, uint16_t value) {
    WRITE(self, value, U16_BE);
}

void fbp_buffer_write_u32_be(struct fbp_buffer_s * self, uint32_t value) {
    WRITE(self, value, U32_BE);
}

void fbp_buffer_write_u64_be(struct fbp_buffer_s * self, uint64_t value) {
    WRITE(self, value, U64_BE);
}

void fbp_buffer_read(struct fbp_buffer_s * self,
                      void * data,
                      fbp_size_t size) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_NOT_NULL(data);
    if (size > 0) {
        FBP_ASSERT(size <= fbp_buffer_read_remaining(self));
        uint8_t * ptr = self->data + self->cursor;
        fbp_memcpy(data, ptr, size);
        self->cursor += (uint16_t) size;
    }
}

#define READ(buffer, ctype, buftype) \
    FBP_DBC_NOT_NULL(buffer); \
    FBP_ASSERT((fbp_size_t) sizeof(ctype) <= fbp_buffer_read_remaining(buffer)); \
    uint8_t * ptr = buffer->data + buffer->cursor; \
    ctype value = FBP_BBUF_DECODE_##buftype (ptr); \
    buffer->cursor += sizeof(ctype); \
    return value;

uint8_t fbp_buffer_read_u8(struct fbp_buffer_s * self) {
    READ(self, uint8_t, U8);
}

uint16_t fbp_buffer_read_u16_le(struct fbp_buffer_s * self) {
    READ(self, uint16_t, U16_LE);
}

uint32_t fbp_buffer_read_u32_le(struct fbp_buffer_s * self) {
    READ(self, uint32_t, U32_LE);
}

uint64_t fbp_buffer_read_u64_le(struct fbp_buffer_s * self) {
    READ(self, uint64_t, U64_LE);
}

uint16_t fbp_buffer_read_u16_be(struct fbp_buffer_s * self) {
    READ(self, uint16_t, U16_BE);
}

uint32_t fbp_buffer_read_u32_be(struct fbp_buffer_s * self) {
    READ(self, uint32_t, U32_BE);
}

uint64_t fbp_buffer_read_u64_be(struct fbp_buffer_s * self) {
    READ(self, uint64_t, U64_BE);
}

void fbp_buffer_erase(struct fbp_buffer_s * self,
                       fbp_size_t start,
                       fbp_size_t end) {
    FBP_DBC_NOT_NULL(self);
    FBP_DBC_RANGE_TYPE(fbp_size_t, start, 0, self->length - 1);
    FBP_DBC_RANGE_TYPE(fbp_size_t, end, 0, self->length);
    fbp_size_t length = end - start;
    if (length > 0) {
        for (fbp_size_t k = start; k < (self->length - length); ++k) {
            self->data[k] = self->data[k + length];
        }
        if (self->cursor >= end) {
            self->cursor -= (uint16_t) length;
        } else if (self->cursor > start) {
            self->cursor = (uint16_t) start;
        }
        self->length -= (uint16_t) length;

    }
}

static void free_static_(struct fbp_buffer_manager_s const * self, struct fbp_buffer_s * buffer) {
    (void) self;
    (void) buffer;
}

const struct fbp_buffer_manager_s fbp_buffer_manager_static = {
        .free = free_static_
};
