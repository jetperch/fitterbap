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

#include "fitterbap/collections/ring_buffer_msg.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"

/*
 * The message storage format is:
 *    sz[7:0], sz[15:8], sz[23:16], sz[31:24], msg[0...N]
 * The message size, sz, must be less than 0x80000000.
 * Any message with the bit[31] set is considered a control header,
 * which indicates wrap_around.
 */

void fbp_rbm_init(struct fbp_rbm_s * self, uint8_t * buffer, uint32_t buffer_size) {
    self->buf = buffer;
    self->buf_size = buffer_size;
    fbp_rbm_clear(self);
}

void fbp_rbm_clear(struct fbp_rbm_s * self) {
    self->head = 0;
    self->tail = 0;
    self->count = 0;
    fbp_memset(self->buf, 0, self->buf_size);
}

static inline uint8_t * add_sz(uint8_t * p, uint32_t sz) {
    p[0] = sz & 0xff;
    p[1] = (sz >> 8) & 0xff;
    p[2] = (sz >> 16) & 0xff;
    p[3] = (sz >> 24) & 0xff;
    return (p + 4);
}

uint8_t * fbp_rbm_alloc(struct fbp_rbm_s * self, uint32_t size) {
    uint8_t *p = self->buf + self->head;
    uint32_t head = self->head;
    uint32_t tail = self->tail;

    if (size > self->buf_size) {
        FBP_LOGE("fbp_rbm_alloc too big");
        return NULL;
    }

    if (head >= tail) {
        uint32_t end_idx = head + 4 + size + 4 + (tail ? 0 : 1);
        if (end_idx < self->buf_size) {
            // fits as is, no wrap
        } else if ((size + 5) < tail) {
            // fits after wrap
            add_sz(p, 0xffffffffU);
            p = self->buf;
        } else if (head == tail) {
            // Big item, but buffer is empty.  Reset pointers to make room.
            self->head = 0;
            self->tail = 0;
            p = self->buf;
        } else {
            return NULL; // does not fit
        }
    } else if ((head + size + 5) < tail) {
        // fits as is
    } else {
        return NULL; // does not fit.
    }
    p = add_sz(p, size);
    head = ((uint32_t) (p - self->buf)) + size;
    if (head >= self->buf_size) {
        FBP_ASSERT(head == self->buf_size);
        head = 0;
    }
    self->head = head;
    ++self->count;
    return p;
}

static inline uint32_t get_sz(uint8_t * p) {
    return ((uint32_t) p[0])
            | (((uint32_t) p[1]) << 8)
            | (((uint32_t) p[2]) << 16)
            | (((uint32_t) p[3]) << 24);
}

uint8_t * fbp_rbm_peek(struct fbp_rbm_s * self, uint32_t * size) {
    uint8_t *p = self->buf + self->tail;
    uint32_t head = self->head;
    uint32_t sz;
    *size = 0;

    if (self->tail == head) {
        return NULL;
    }
    sz = get_sz(p);
    if (sz >= 0x80000000) {
        // rollover
        if (head > self->tail) {
            FBP_LOGE("buffer overflow"); // should never be possible
            fbp_rbm_clear(self);
            return NULL;
        }
        self->tail = 0;
        if (self->tail == head) {
            return NULL;
        }
        p = self->buf;
        sz = get_sz(p);
    }
    *size = sz;
    return (p + 4);
}


uint8_t * fbp_rbm_pop(struct fbp_rbm_s * self, uint32_t * size) {
    uint8_t *p = fbp_rbm_peek(self, size);
    uint32_t tail = self->tail;
    if (p) {
        tail += (4 + *size);
        if (tail >= self->buf_size) {
            tail -= self->buf_size;
        }
        if (self->count) {
            --self->count;
        }
    }
    self->tail = tail;
    return p;
}
