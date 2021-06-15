/*
 * Copyright 2017-2021 Jetperch LLC
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
 
#include "fitterbap/event_manager.h"
#include "fitterbap/ec.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/time.h"
#include "fitterbap/platform.h"
#include <stdlib.h>


struct event_s {
    int32_t event_id;
    int64_t timestamp;
    fbp_evm_callback cbk_fn;
    void * cbk_user_data;
    struct fbp_list_s node;
};

#define EVGET(item) fbp_list_entry(item, struct event_s, node)

struct fbp_evm_s {
    int32_t event_counter;
    fbp_os_mutex_t mutex;
    fbp_evm_on_schedule on_schedule_fn;
    void * on_schedule_user_data;
    struct fbp_list_s events_pending;
    struct fbp_list_s events_free;
};

static inline void lock(struct fbp_evm_s * self) {
    fbp_os_mutex_lock(self->mutex);
}

static inline void unlock(struct fbp_evm_s * self) {
    fbp_os_mutex_unlock(self->mutex);
}

struct fbp_evm_s * fbp_evm_allocate() {
    struct fbp_evm_s * self = fbp_alloc_clr(sizeof(struct fbp_evm_s));
    if (self) {
        fbp_list_initialize(&self->events_pending);
        fbp_list_initialize(&self->events_free);
    }
    return self;
}

static void event_list_free(struct fbp_list_s * list) {
    struct fbp_list_s * item;
    struct event_s * ev;
    fbp_list_foreach(list, item) {
        ev = EVGET(item);
        fbp_free(ev);
    }
    fbp_list_initialize(list);
}

void fbp_evm_free(struct fbp_evm_s * self) {
    if (self) {
        fbp_os_mutex_t mutex = self->mutex;
        lock(self);
        event_list_free(&self->events_pending);
        event_list_free(&self->events_free);
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
        }
    }
}

int32_t fbp_evm_schedule(struct fbp_evm_s * self, int64_t timestamp,
                          fbp_evm_callback cbk_fn, void * cbk_user_data) {
    struct event_s * ev;
    if (!cbk_fn) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    lock(self);
    if (fbp_list_is_empty(&self->events_free)) {
        ++self->event_counter;
        ev = fbp_alloc_clr(sizeof(struct event_s));
        ev->event_id = self->event_counter;
    } else {
        ev = EVGET(fbp_list_remove_head(&self->events_free));
    }
    fbp_list_initialize(&ev->node);
    ev->timestamp = timestamp;
    ev->cbk_fn = cbk_fn;
    ev->cbk_user_data = cbk_user_data;

    struct fbp_list_s * node;
    struct event_s * ev_next;
    int count = 0;
    fbp_list_foreach(&self->events_pending, node) {
        ev_next = EVGET(node);
        if (ev->timestamp < ev_next->timestamp) {
            fbp_list_insert_before(node, &ev->node);
            unlock(self);
            if ((count == 0) && self->on_schedule_fn) {
                self->on_schedule_fn(self->on_schedule_user_data, ev->timestamp);
            }
            return ev->event_id;
        }
        ++count;
    }
    fbp_list_add_tail(&self->events_pending, &ev->node);
    unlock(self);
    if ((count == 0) && self->on_schedule_fn) {
        self->on_schedule_fn(self->on_schedule_user_data, ev->timestamp);
    }
    return ev->event_id;
}

int32_t fbp_evm_cancel(struct fbp_evm_s * self, int32_t event_id) {
    struct fbp_list_s * node;
    struct event_s * ev;
    lock(self);
    fbp_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->event_id == event_id) {
            fbp_list_remove(node);
            ev->cbk_fn = NULL;
            fbp_list_add_tail(&self->events_free, node);
            break;
        }
    }
    unlock(self);
    return 0;
}

int64_t fbp_evm_time_next(struct fbp_evm_s * self) {
    int64_t rv;
    lock(self);
    if (fbp_list_is_empty(&self->events_pending)) {
        rv = FBP_TIME_MAX;
    } else {
        rv = EVGET(fbp_list_peek_head(&self->events_pending))->timestamp;
    }
    unlock(self);
    return rv;
}

int64_t fbp_evm_interval_next(struct fbp_evm_s * self, int64_t time_current) {
    lock(self);
    if (fbp_list_is_empty(&self->events_pending)) {
        unlock(self);
        return FBP_TIME_MAX;
    }
    struct event_s * ev = EVGET(fbp_list_peek_head(&self->events_pending));
    if (ev->timestamp <= time_current) {
        unlock(self);
        return 0;
    } else {
        unlock(self);
        return ev->timestamp - time_current;
    }
}

int32_t fbp_evm_scheduled_event_count(struct fbp_evm_s * self) {
    lock(self);
    int32_t count = fbp_list_length(&self->events_pending);
    unlock(self);
    return count;
}

int32_t fbp_evm_process(struct fbp_evm_s * self, int64_t time_current) {
    struct fbp_list_s * node;
    struct event_s * ev;
    int32_t count = 0;
    lock(self);
    fbp_list_foreach(&self->events_pending, node) {
        ev = EVGET(node);
        if (ev->timestamp > time_current) {
            break;
        }
        fbp_list_remove(node);
        unlock(self);
        ev->cbk_fn(ev->cbk_user_data, ev->event_id);
        lock(self);
        ev->cbk_fn = NULL;
        fbp_list_add_tail(&self->events_free, node);
        ++count;
    }
    unlock(self);
    return count;
}

static int64_t timestamp_default(struct fbp_evm_s * self) {
    (void) self;
    return fbp_time_rel();
}

void fbp_evm_register_mutex(struct fbp_evm_s * self, fbp_os_mutex_t mutex) {
    self->mutex = mutex;
}

void fbp_evm_register_schedule_callback(struct fbp_evm_s * self,
                                        fbp_evm_on_schedule cbk_fn, void * cbk_user_data) {
    lock(self);
    self->on_schedule_fn = cbk_fn;
    self->on_schedule_user_data = cbk_user_data;
    unlock(self);
}

int32_t fbp_evm_api_get(struct fbp_evm_s * self, struct fbp_evm_api_s * api) {
    if (!self || !api) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    api->evm = self;
    api->timestamp = timestamp_default;
    api->schedule = (fbp_evm_schedule_fn) fbp_evm_schedule;
    api->cancel = (fbp_evm_cancel_fn) fbp_evm_cancel;
    return 0;
}
