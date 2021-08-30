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

#include "fitterbap/logh.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/memory/bbuf.h"
#include "fitterbap/os/task.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/cdef.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/time.h"
#include "tinyprintf.h"
#include <stdarg.h>


struct msg_s {
    struct fbp_list_s item;
    struct fbp_logh_header_s header;
    char filename[FBP_LOGH_FILENAME_SIZE_MAX];
    char message[FBP_LOGH_MESSAGE_SIZE_MAX];
};

struct dispatch_s {
    fbp_logh_recv fn;
    void * user_data;
};

struct fbp_logh_s {
    char origin_prefix;
    uint8_t dispatch_idx;
    fbp_logh_on_publish on_publish_fn;
    void * on_publish_user_data;
    struct dispatch_s dispatch[FBP_LOGH_DISPATCH_MAX];
    struct fbp_list_s msg_free;
    struct fbp_list_s msg_pend;
    struct msg_s * msg_alloc_ptr;
    fbp_os_mutex_t mutex;
    int64_t (*time_fn)();
};

static struct fbp_logh_s * singleton_ = NULL;


static inline struct fbp_logh_s * resolve_instance(struct fbp_logh_s * self) {
    return self ? self : singleton_;
}


static inline void lock(struct fbp_logh_s * self) {
    fbp_os_mutex_lock(self->mutex);
}

static inline void unlock(struct fbp_logh_s * self) {
    fbp_os_mutex_unlock(self->mutex);
}

static const char * find_basename(const char * filename) {
    const char * p = filename;
    while (*filename) {
        if (*filename == '/') {
            p = filename + 1;
        }
        ++filename;
    }
    return p;
}

int32_t fbp_logh_publish(struct fbp_logh_s * self, uint8_t level, const char * filename, uint32_t line, const char * format, ...) {
    int32_t rc = 0;
    va_list args;
    char * p;
    self = resolve_instance(self);
    if ((level > 0x0f) || (line >= 0x100000)) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if (!self) {
        return FBP_ERROR_UNAVAILABLE;
    }
    filename = find_basename(filename);

    lock(self);
    if (fbp_list_is_empty(&self->msg_free)) {
        rc = FBP_ERROR_FULL;
    } else {
        struct msg_s * msg = (struct msg_s *) fbp_list_remove_head(&self->msg_free);
        msg->header.version = FBP_LOGH_VERSION;
        msg->header.level = level;
        msg->header.origin_prefix = self->origin_prefix;
        msg->header.origin_thread = 0;
        msg->header.line = line;
        msg->header.timestamp = self->time_fn();
        p = msg->filename;
        for (int i = 0; (i < (FBP_LOGH_FILENAME_SIZE_MAX - 1)) && (*filename); ++i) {
            *p++ = *filename++;
        }
        *p = 0;
        va_start(args, format);
        tfp_vsnprintf(msg->message, FBP_LOGH_MESSAGE_SIZE_MAX, format, args);
        va_end(args);

        fbp_list_add_tail(&self->msg_pend, &msg->item);
        if (self->on_publish_fn) {
            self->on_publish_fn(self->on_publish_user_data);
        }
    }
    unlock(self);
    return rc;
}

int32_t fbp_logh_publish_formatted(struct fbp_logh_s * self, struct fbp_logh_header_s const * header,
                                   const char * filename, const char * message) {
    char * p;
    int32_t rc = 0;
    self = resolve_instance(self);
    if (!self) {
        return FBP_ERROR_UNAVAILABLE;
    }

    lock(self);
    if (fbp_list_is_empty(&self->msg_free)) {
        rc = FBP_ERROR_FULL;
    } else {
        struct msg_s * msg = (struct msg_s *) fbp_list_remove_head(&self->msg_free);
        msg->header = *header;
        p = msg->filename;
        for (int i = 0; (i < (FBP_LOGH_FILENAME_SIZE_MAX - 1)) && (*filename); ++i) {
            *p++ = *filename++;
        }
        *p = 0;
        p = msg->message;
        for (int i = 0; (i < (FBP_LOGH_MESSAGE_SIZE_MAX - 1)) && (*message); ++i) {
            *p++ = *message++;
        }
        *p = 0;
        fbp_list_add_tail(&self->msg_pend, &msg->item);
        if (self->on_publish_fn) {
            self->on_publish_fn(self->on_publish_user_data);
        }
    }
    unlock(self);
    return rc;
}

FBP_API int32_t fbp_logh_dispatch_register(struct fbp_logh_s * self, fbp_logh_recv fn, void * user_data) {
    int32_t rc = FBP_ERROR_FULL;
    self = resolve_instance(self);
    lock(self);
    for (int i = 0; i < FBP_LOGH_DISPATCH_MAX; ++i) {
        if (!self->dispatch[i].fn) {
            self->dispatch[i].user_data = user_data;
            self->dispatch[i].fn = fn;
            rc = 0;
            break;
        }
    }
    unlock(self);
    return rc;
}

FBP_API int32_t fbp_logh_dispatch_unregister(struct fbp_logh_s * self, fbp_logh_recv fn, void * user_data) {
    int32_t rc = FBP_ERROR_NOT_FOUND;
    self = resolve_instance(self);
    lock(self);
    for (int i = 0; i < FBP_LOGH_DISPATCH_MAX; ++i) {
        if ((self->dispatch[i].fn == fn) && (self->dispatch[i].user_data == user_data)) {
            self->dispatch[i].fn = NULL;
            self->dispatch[i].user_data = NULL;
            rc = 0;
        }
    }
    unlock(self);
    return rc;
}

FBP_API void fbp_logh_dispatch_unregister_all(struct fbp_logh_s * self) {
    self = resolve_instance(self);
    lock(self);
    for (int i = 0; i < FBP_LOGH_DISPATCH_MAX; ++i) {
        self->dispatch[i].fn = NULL;
        self->dispatch[i].user_data = NULL;
    }
    unlock(self);
}

FBP_API void fbp_logh_publish_register(struct fbp_logh_s * self, fbp_logh_on_publish fn, void * user_data) {
    self = resolve_instance(self);
    lock(self);
    self->on_publish_fn = NULL;
    self->on_publish_user_data = user_data;
    self->on_publish_fn = fn;
    unlock(self);
}

int32_t fbp_logh_process(struct fbp_logh_s * self) {
    struct msg_s * msg;
    int32_t rc;
    self = resolve_instance(self);
    if (!self) {
        return 0;
    }
    while (!fbp_list_is_empty(&self->msg_pend)) {
        msg = (struct msg_s *) fbp_list_peek_head(&self->msg_pend);
        for (uint8_t i = self->dispatch_idx; i < FBP_LOGH_DISPATCH_MAX; ++i) {
            if (self->dispatch[i].fn) {
                rc = self->dispatch[i].fn(self->dispatch[i].user_data, &msg->header, msg->filename, msg->message);
                if (rc == FBP_ERROR_FULL) {
                    return rc;
                }
            }
            ++self->dispatch_idx;
        }

        // Message dispatched to all recipients, pop
        self->dispatch_idx = 0;
        lock(self);
        msg = (struct msg_s *) fbp_list_remove_head(&self->msg_pend);
        fbp_list_add_head(&self->msg_free, &msg->item);
        unlock(self);
    }
    return 0;
}

struct fbp_logh_s * fbp_logh_initialize(char origin_prefix, uint32_t msg_buffers_max, int64_t (*time_fn)()) {
    struct fbp_logh_s * self = fbp_alloc_clr(sizeof(struct fbp_logh_s));
    self->mutex = fbp_os_mutex_alloc();
    self->origin_prefix = origin_prefix;
    fbp_list_initialize(&self->msg_free);
    fbp_list_initialize(&self->msg_pend);

    if (!time_fn) {
        self->time_fn = fbp_time_utc;
    } else {
        self->time_fn = time_fn;
    }

    if (msg_buffers_max) {
        self->msg_alloc_ptr = fbp_alloc_clr(msg_buffers_max * sizeof(struct msg_s));
        for (uint32_t i = 0; i < msg_buffers_max; ++i) {
            struct msg_s * msg = &self->msg_alloc_ptr[i];
            fbp_list_initialize(&msg->item);
            fbp_list_add_tail(&self->msg_free, &msg->item);
        }
    }

    if (!singleton_) {
        singleton_ = self;
    }

    return self;
}

void fbp_logh_finalize(struct fbp_logh_s * self) {
    if (self) {
        if (singleton_ == self) {
            singleton_ = NULL;
        }
        lock(self);
        fbp_os_mutex_t mutex = self->mutex;
        if (self->msg_alloc_ptr) {
            fbp_free(self->msg_alloc_ptr);
            self->msg_alloc_ptr = 0;
        }
        fbp_list_initialize(&self->msg_free);
        fbp_list_initialize(&self->msg_pend);
        fbp_free(self);
        fbp_os_mutex_unlock(mutex);
        fbp_os_mutex_free(mutex);
    }
}
