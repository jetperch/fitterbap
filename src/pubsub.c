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

#define FBP_LOG_LEVEL FBP_LOG_LEVEL_NOTICE
#include "fitterbap/pubsub.h"
#include "fitterbap/collections/ring_buffer_msg.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/platform.h"
#include "fitterbap/topic_list.h"
#include "fitterbap/collections/list.h"
#include "fitterbap/cstr.h"

enum op_e {
    OP_PUBLISH,
    OP_SUBSCRIBE,
};

struct subscriber_s {
    fbp_pubsub_subscribe_fn cbk_fn;
    void * cbk_user_data;
    uint8_t flags;
    struct fbp_list_s item;
};

struct topic_s {
    struct fbp_union_s value;
    struct topic_s * parent;
    const char * meta;
    struct fbp_list_s item;  // used by parent->children list
    struct fbp_list_s children;
    struct fbp_list_s subscribers;
    char name[FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL];
};

struct message_s {
    char name[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    struct fbp_union_s value;
    fbp_pubsub_subscribe_fn src_fn;
    void * src_user_data;
    struct fbp_list_s item;
};

struct fbp_pubsub_s {
    char topic_prefix[FBP_PUBSUB_TOPIC_LENGTH_MAX];     // The top-level topic name for this instance
    struct fbp_topic_list_s topic_list;
    fbp_pubsub_on_publish_fn cbk_fn;
    void * cbk_user_data;
    fbp_os_mutex_t mutex;
    struct topic_s * root_topic;
    struct fbp_list_s subscriber_free;
    struct fbp_list_s msg_pend;
    struct fbp_list_s msg_free;

    struct fbp_rbm_s mrb;                               // for mutable message payloads
    uint8_t buffer[];
};

const char RESERVED_SUFFIX[] = "/?#$'\"\\`&@%";

static uint8_t publish(struct topic_s * topic, struct message_s * msg);

static inline void lock(struct fbp_pubsub_s * self) {
    if (self->mutex) {
        fbp_os_mutex_lock(self->mutex);
    }
}

static inline void unlock(struct fbp_pubsub_s * self) {
    if (self->mutex) {
        fbp_os_mutex_unlock(self->mutex);
    }
}

static struct message_s * msg_alloc(struct fbp_pubsub_s * self) {
    struct message_s * msg;
    lock(self);
    if (fbp_list_is_empty(&self->msg_free)) {
        msg = fbp_alloc(sizeof(struct message_s));
    } else {
        struct fbp_list_s * item = fbp_list_remove_head(&self->msg_free);
        msg = FBP_CONTAINER_OF(item, struct message_s, item);
    }
    fbp_list_initialize(&msg->item);
    msg->name[0] = 0;
    msg->value.op = OP_PUBLISH;
    msg->value.type = FBP_UNION_NULL;
    msg->value.size = 0;
    msg->src_fn = NULL;
    msg->src_user_data = NULL;
    unlock(self);
    return msg;
}

static void msg_free(struct fbp_pubsub_s * self, struct message_s * msg) {
    lock(self);
    fbp_list_add_tail(&self->msg_free, &msg->item);
    unlock(self);
}

static struct subscriber_s * subscriber_alloc(struct fbp_pubsub_s * self) {
    struct subscriber_s * sub;
    if (!fbp_list_is_empty(&self->subscriber_free)) {
        struct fbp_list_s * item;
        item = fbp_list_remove_head(&self->subscriber_free);
        sub = FBP_CONTAINER_OF(item, struct subscriber_s, item);
    } else {
        sub = fbp_alloc_clr(sizeof(struct subscriber_s));
        FBP_LOGD3("subscriber alloc: %p", (void *) sub);
    }
    fbp_list_initialize(&sub->item);
    sub->flags = 0;
    sub->cbk_fn = NULL;
    sub->cbk_user_data = NULL;
    return sub;
}

static void subscriber_free(struct fbp_pubsub_s * self, struct subscriber_s * sub) {
    fbp_list_add_tail(&self->subscriber_free, &sub->item);
}

static bool is_reserved_char(char ch) {
    for (const char * p = RESERVED_SUFFIX; *p; ++p) {
        if (ch == *p) {
            return true;
        }
    }
    return false;
}

static bool topic_name_set(struct topic_s * topic, const char * name) {
    const char * name_orig = name;
    (void) name_orig;  // when logging is off
    for (int i = 0; i < FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL; ++i) {
        if (*name) {
            topic->name[i] = *name++;
        } else {
            topic->name[i] = 0;
            return true;
        }
    }
    FBP_LOGW("topic name truncated: %s", name_orig);
    topic->name[FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL - 1] = 0;
    return false;
}

static bool topic_str_copy(char * topic_str, const char * src, size_t * str_len) {
    size_t sz = 0;
    while (src[sz]) {
        if (sz >= FBP_PUBSUB_TOPIC_LENGTH_MAX) {
            topic_str[FBP_PUBSUB_TOPIC_LENGTH_MAX - 1] = 0;  // truncate
            FBP_LOGW("topic too long: %s", src);
            if (str_len) {
                *str_len = FBP_PUBSUB_TOPIC_LENGTH_MAX;
            }
            return false;
        }
        topic_str[sz] = src[sz];
        ++sz;
    }
    topic_str[sz] = 0;
    if (str_len) {
        *str_len = sz;
    }
    return true;
}

static void topic_str_append(char * topic_str, const char * topic_sub_str) {
    // WARNING: topic_str must be >= TOPIC_LENGTH_MAX
    // topic_sub_str <= TOPIC_LENGTH_PER_LEVEL
    size_t topic_len = 0;
    char * t = topic_str;

    // find the end of topic_str
    while (*t) {
        ++topic_len;
        ++t;
    }
    if (topic_len >= (FBP_PUBSUB_TOPIC_LENGTH_MAX - 1)) {
        return;
    }

    // add separator
    if (topic_len) {
        *t++ = '/';
        ++topic_len;
    }

    // Copy substring
    while (*topic_sub_str && (topic_len < (FBP_PUBSUB_TOPIC_LENGTH_MAX - 1))) {
        *t++ = *topic_sub_str++;
        ++topic_len;
    }
    *t = 0;  // null terminate
}

static void topic_str_pop(char * topic_str) {
    char * end = topic_str;
    if (!topic_str || !topic_str[0]) {
        return;  // nothing to pop
    }
    for (int i = 1; i < FBP_PUBSUB_TOPIC_LENGTH_MAX; ++i) {
        if (!topic_str[i]) {
            *end = topic_str[i - 1];
        }
    }
    while (end >= topic_str) {
        if (*end == '/') {
            *end-- = 0;
            return;
        }
        *end-- = 0;
    }
}

static struct topic_s * topic_alloc(struct fbp_pubsub_s * self, const char * name) {
    (void) self;
    struct topic_s * topic = fbp_alloc_clr(sizeof(struct topic_s));
    topic->value.type = FBP_UNION_NULL;
    fbp_list_initialize(&topic->item);
    fbp_list_initialize(&topic->children);
    fbp_list_initialize(&topic->subscribers);
    topic_name_set(topic, name);
    FBP_LOGD3("topic alloc: %p", (void *)topic);
    return topic;
}

static void topic_free(struct fbp_pubsub_s * self, struct topic_s * topic) {
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    fbp_list_foreach(&topic->subscribers, item) {
        subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
        fbp_list_remove(item);
        subscriber_free(self, subscriber);
    }
    struct topic_s * subtopic;
    fbp_list_foreach(&topic->children, item) {
        subtopic = FBP_CONTAINER_OF(item, struct topic_s, item);
        fbp_list_remove(item);
        topic_free(self, subtopic);
    }
    FBP_LOGD3("topic free: %p", (void *)topic);
    fbp_free(topic);
}

/**
 * @brief Parse the next subtopic.
 * @param topic[inout] The topic, which is advanced to the next subtopic.
 * @param subtopic[out] The parsed subtopic, which must be
 *      at least FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL bytes.
 * @return true on success, false on failure.
 */
static bool subtopic_get_str(const char ** topic, char * subtopic) {
    const char * t = *topic;
    for (int i = 0; i < FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL; ++i) {
        if (*t == 0) {
            *subtopic = 0;
            *topic = t;
            return true;
        } else if (*t == '/') {
            *subtopic = 0;
            t++;
            *topic = t;
            return true;
        } else {
            *subtopic++ = *t++;
        }
    }
    FBP_LOGW("subtopic too long: %s", *topic);
    return false;
}

static struct topic_s * subtopic_find(struct topic_s * parent, const char * subtopic_str) {
    struct fbp_list_s * item;
    struct topic_s * topic;
    fbp_list_foreach(&parent->children, item) {
        topic = FBP_CONTAINER_OF(item, struct topic_s, item);
        if (0 == strcmp(subtopic_str, topic->name)) {
            return topic;
        }
    }
    return NULL;
}

static struct topic_s * topic_find(struct fbp_pubsub_s * self, const char * topic, bool create) {
    char subtopic_str[FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL];
    const char * c = topic;

    struct topic_s * t = self->root_topic;
    struct topic_s * subtopic;
    while (*c != 0) {
        if (!subtopic_get_str(&c, subtopic_str)) {
            return NULL;
        }
        subtopic = subtopic_find(t, subtopic_str);
        if (!subtopic) {
            if (!create) {
                return NULL;
            }
            FBP_LOGD1("%s: create new topic %s", topic, subtopic_str);
            subtopic = topic_alloc(self, subtopic_str);
            subtopic->parent = t;
            fbp_list_add_tail(&t->children, &subtopic->item);
        }
        t = subtopic;
    }
    return t;
}

/**
 * @brief Find a topic most closely matching a topic string.
 *
 * @param self The instance.
 * @param topic The topic string.
 * @return The matching topic instance.
 *
 * Pop hierarchy levels off the topic string until we find a matching
 * topic.  In the worst case, we return the root topic.
 */
static struct topic_s * topic_find_existing_base(struct fbp_pubsub_s * self, const char * topic) {
    char str_start[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    size_t name_sz = 0;
    struct topic_s * t = NULL;
    if (!topic_str_copy(str_start, topic, &name_sz)) {
        return NULL;
    }
    char * str_end = str_start + name_sz - 1;

    if (is_reserved_char(*str_end)) {
        *str_end-- = 0;
    }

    // find longest matching topic that exists
    t = topic_find(self, str_start, false);
    while (!t && *str_start) {
        topic_str_pop(str_start);
        t = topic_find(self, str_start, false);
    }
    FBP_ASSERT(t);
    return t;
}

static void topic_list_update(struct fbp_pubsub_s * self, bool do_publish) {
    struct topic_s * t = topic_find(self, FBP_PUBSUB_TOPIC_LIST, true);
    t->value.type = FBP_UNION_STR;
    t->value.flags = FBP_UNION_FLAG_RETAIN;
    t->value.value.str = self->topic_list.topic_list;
    t->value.size = (uint32_t) (strlen(self->topic_list.topic_list) + 1);

    // must manually call publish since "unchanged"
    struct message_s msg = {
        .name = FBP_PUBSUB_TOPIC_LIST,
        .value = t->value,
        .src_fn = NULL,
        .src_user_data = NULL,
    };
    if (do_publish) {
        publish(t, &msg);
    }
}

static uint8_t on_topic_add(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct fbp_pubsub_s * self = (struct fbp_pubsub_s *) user_data;
    fbp_topic_list_append(&self->topic_list, value->value.str);
    topic_list_update(self, true);
    return 0;
}

static uint8_t on_topic_remove(void * user_data, const char * topic, const struct fbp_union_s * value) {
    (void) topic;
    struct fbp_pubsub_s * self = (struct fbp_pubsub_s *) user_data;
    fbp_topic_list_remove(&self->topic_list, value->value.str);
    topic_list_update(self, true);
    return 0;
}

struct fbp_pubsub_s * fbp_pubsub_initialize(const char * topic_prefix, uint32_t buffer_size) {
    FBP_LOGI("initialize");
    struct fbp_pubsub_s * self = (struct fbp_pubsub_s *) fbp_alloc_clr(sizeof(struct fbp_pubsub_s) + buffer_size);
    fbp_cstr_copy(self->topic_prefix, topic_prefix, fbp_sizeof(self->topic_prefix));
    fbp_topic_list_clear(&self->topic_list);
    fbp_topic_list_append(&self->topic_list, topic_prefix);
    self->root_topic = topic_alloc(self, "");
    fbp_list_initialize(&self->subscriber_free);
    fbp_list_initialize(&self->msg_pend);
    fbp_list_initialize(&self->msg_free);
    fbp_rbm_init(&self->mrb, self->buffer, buffer_size);

    struct topic_s * t;
    struct subscriber_s * sub;
    t = topic_find(self, FBP_PUBSUB_TOPIC_PREFIX, true);
    t->value.type = FBP_UNION_STR;
    t->value.flags = FBP_UNION_FLAG_RETAIN;
    t->value.value.str = self->topic_prefix;
    t->value.size = (uint32_t) (strlen(self->topic_prefix) + 1);

    topic_list_update(self, false);

    t = topic_find(self, FBP_PUBSUB_TOPIC_ADD, true);
    sub = subscriber_alloc(self);
    sub->cbk_fn = on_topic_add;
    sub->cbk_user_data = self;
    fbp_list_add_tail(&t->subscribers, &sub->item);

    t = topic_find(self, FBP_PUBSUB_TOPIC_REMOVE, true);
    sub = subscriber_alloc(self);
    sub->cbk_fn = on_topic_remove;
    sub->cbk_user_data = self;
    fbp_list_add_tail(&t->subscribers, &sub->item);

    return self;
}

void subscriber_list_free(struct fbp_list_s * list) {
    struct fbp_list_s * item;
    struct subscriber_s * sub;
    fbp_list_foreach(list, item) {
        sub = FBP_CONTAINER_OF(item, struct subscriber_s, item);
        fbp_free(sub);
    }
    fbp_list_initialize(list);
}

void msg_list_free(struct fbp_list_s * list) {
    struct fbp_list_s * item;
    struct message_s * msg;
    fbp_list_foreach(list, item) {
        msg = FBP_CONTAINER_OF(item, struct message_s, item);
        fbp_free(msg);
    }
    fbp_list_initialize(list);
}

void fbp_pubsub_finalize(struct fbp_pubsub_s * self) {
    FBP_LOGI("finalize");

    if (self) {
        fbp_os_mutex_t mutex = self->mutex;
        lock(self);
        topic_free(self, self->root_topic);
        subscriber_list_free(&self->subscriber_free);
        msg_list_free(&self->msg_pend);
        msg_list_free(&self->msg_free);
        fbp_free(self);
        if (mutex) {
            fbp_os_mutex_unlock(mutex);
        }
    }
}

const char * fbp_pubsub_topic_prefix(struct fbp_pubsub_s * self) {
    return self->topic_prefix;
}

void fbp_pubsub_register_on_publish(struct fbp_pubsub_s * self,
                                     fbp_pubsub_on_publish_fn cbk_fn, void * cbk_user_data) {
    self->cbk_fn = cbk_fn;
    self->cbk_user_data = cbk_user_data;
}

static void subscribe_traverse(struct topic_s * topic, char * topic_str, fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    size_t topic_str_len = strlen(topic_str);
    char * topic_str_last = topic_str + topic_str_len;
    if ((topic->value.type != FBP_UNION_NULL) && (topic->value.flags & FBP_UNION_FLAG_RETAIN)) {
        cbk_fn(cbk_user_data, topic_str, &topic->value);
    }
    struct fbp_list_s * item;
    struct topic_s * subtopic;
    fbp_list_foreach(&topic->children, item) {
        subtopic = FBP_CONTAINER_OF(item, struct topic_s, item);
        topic_str_append(topic_str, subtopic->name);
        subscribe_traverse(subtopic, topic_str, cbk_fn, cbk_user_data);
        *topic_str_last = 0;  // reset string to original
    }
}

static int32_t msg_enqueue(struct fbp_pubsub_s * self, struct message_s * msg) {
    lock(self);
    fbp_list_add_tail(&self->msg_pend, &msg->item);
    unlock(self);
    if (self->cbk_fn) {
        self->cbk_fn(self->cbk_user_data);
    }
    return 0;
}

int32_t fbp_pubsub_subscribe(struct fbp_pubsub_s * self, const char * topic,
        uint8_t flags, fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    if (!self || !cbk_fn) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if ((flags & (FBP_PUBSUB_SFLAG_REQ | FBP_PUBSUB_SFLAG_RSP)) && topic[0]) {
        FBP_LOGW("req | rsp subscribers must only subscribe to root");
        return FBP_ERROR_PARAMETER_INVALID;
    }

    FBP_LOGI("subscribe \"%s\"", topic);

    struct message_s * msg = msg_alloc(self);
    if (!topic_str_copy(msg->name, topic, NULL)) {
        msg_free(self, msg);
        return FBP_ERROR_PARAMETER_INVALID;
    }

    msg->src_fn = cbk_fn;
    msg->src_user_data = cbk_user_data;
    msg->value.type = FBP_UNION_U32;
    msg->value.flags = 0;
    msg->value.op = OP_SUBSCRIBE;
    msg->value.value.u32 = flags;
    return msg_enqueue(self, msg);
}

int32_t fbp_pubsub_unsubscribe(struct fbp_pubsub_s * self, const char * topic,
                                fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    struct topic_s * t = topic_find(self, topic, false);
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    int count = 0;
    if (!t) {
        return FBP_ERROR_NOT_FOUND;
    }
    lock(self);
    fbp_list_foreach(&t->subscribers, item) {
        subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
        if ((subscriber->cbk_fn == cbk_fn) && (subscriber->cbk_user_data == cbk_user_data)) {
            fbp_list_remove(item);
            subscriber_free(self, subscriber);
            ++count;
        }
    }
    unlock(self);
    if (!count) {
        return FBP_ERROR_NOT_FOUND;
    }
    return 0;
}

static void unsubscribe_traverse(struct fbp_pubsub_s * self, struct topic_s * topic, fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    struct fbp_list_s * item;
    struct topic_s * subtopic;
    struct subscriber_s * subscriber;
    fbp_list_foreach(&topic->subscribers, item) {
        subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
        if ((subscriber->cbk_fn == cbk_fn) && (subscriber->cbk_user_data == cbk_user_data)) {
            fbp_list_remove(item);
            subscriber_free(self, subscriber);
        }
    }

    fbp_list_foreach(&topic->children, item) {
        subtopic = FBP_CONTAINER_OF(item, struct topic_s, item);
        unsubscribe_traverse(self, subtopic, cbk_fn, cbk_user_data);
    }
}

int32_t fbp_pubsub_unsubscribe_from_all(struct fbp_pubsub_s * self,
                               fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data) {
    struct topic_s * t = self->root_topic;
    lock(self);
    unsubscribe_traverse(self, t, cbk_fn, cbk_user_data);
    unlock(self);
    return 0;
}

static bool is_ptr_type(uint8_t type) {
    switch (type) {
        case FBP_UNION_STR:   // intentional fall-through
        case FBP_UNION_JSON:  // intentional fall-through
        case FBP_UNION_BIN:
            return true;
        default:
            return false;
    }
}

static bool is_str_type(uint8_t type) {
    switch (type) {
        case FBP_UNION_STR:   // intentional fall-through
        case FBP_UNION_JSON:  // intentional fall-through
            return true;
        default:
            return false;
    }
}

int32_t fbp_pubsub_publish(struct fbp_pubsub_s * self,
        const char * topic, const struct fbp_union_s * value,
        fbp_pubsub_subscribe_fn src_fn, void * src_user_data) {

    bool do_copy = false;
    uint32_t size = value->size;
    if (is_ptr_type(value->type)) {
        if ((!value->size) && is_str_type(value->type)) {
            size_t sz = strlen(value->value.str) + 1;
            if (sz > UINT32_MAX) {
                return FBP_ERROR_TOO_BIG;
            }
            size = (uint32_t) sz;
        }
        if (0 == (value->flags & FBP_UNION_FLAG_CONST)) {
            if (value->flags & FBP_UNION_FLAG_RETAIN) {
                FBP_LOGE("non-const retained ptr not allowed");
                return FBP_ERROR_PARAMETER_INVALID;
            }
            do_copy = true;
            if (size > (self->mrb.buf_size / 2)) {
                FBP_LOGE("too big for available buffer");
                return FBP_ERROR_PARAMETER_INVALID;
            }
        }
    } else {
        size = 0;
    }

    struct message_s * msg = msg_alloc(self);
    if (!topic_str_copy(msg->name, topic, NULL)) {
        msg_free(self, msg);
        return FBP_ERROR_PARAMETER_INVALID;
    }

    msg->src_fn = src_fn;
    msg->src_user_data = src_user_data;
    msg->value = *value;
    msg->value.op = OP_PUBLISH;
    msg->value.size = size;
    if (do_copy && size) {
        lock(self);
        uint8_t *buf = fbp_rbm_alloc(&self->mrb, size);
        if (!buf) { // full!
            fbp_list_add_tail(&self->msg_free, &msg->item);
            msg_free(self, msg);
            unlock(self);
            return FBP_ERROR_NOT_ENOUGH_MEMORY;
        }
        fbp_memcpy(buf, value->value.str, size);
        msg->value.value.bin = buf;
        unlock(self);
    }
    return msg_enqueue(self, msg);
}

int32_t fbp_pubsub_meta(struct fbp_pubsub_s * self, const char * topic, const char * meta_json) {
    size_t sz = 0;
    struct message_s * msg = msg_alloc(self);
    if (!topic_str_copy(msg->name, topic, &sz)) {
        msg_free(self, msg);
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if (sz && (msg->name[sz - 1] != '$')) {
        // Add '$'
        if ((sz + 2) > FBP_PUBSUB_TOPIC_LENGTH_MAX) {
            msg_free(self, msg);
            return FBP_ERROR_PARAMETER_INVALID;
        }
        msg->name[sz] = '$';
        msg->name[sz + 1] = 0;
    }
    if (meta_json) {
        msg->value.type = FBP_UNION_JSON;
        msg->value.flags = FBP_UNION_FLAG_CONST | FBP_UNION_FLAG_RETAIN;
        msg->value.value.str = meta_json;
        msg->value.size = (uint32_t) (strlen(meta_json) + 1);
    } else {
        msg->value.type = FBP_UNION_NULL;
        msg->value.flags = 0;
        msg->value.size = 0;
    }
    return msg_enqueue(self, msg);
}

int32_t fbp_pubsub_query(struct fbp_pubsub_s * self, const char * topic, struct fbp_union_s * value) {
    lock(self);  // fbp_list_foreach not thread-safe.  Need mutex.
    struct topic_s * t = topic_find(self, topic, false);
    unlock(self);
    if (!t || (0 == (t->value.flags & FBP_UNION_FLAG_RETAIN))) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    if (value) {
        *value = t->value;
    }
    return 0;
}

static void metadata_req_forward(struct fbp_pubsub_s * self, struct message_s * msg) {
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    struct topic_s * t = self->root_topic;
    fbp_list_foreach(&t->subscribers, item) {
        subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
        if (subscriber->flags & FBP_PUBSUB_SFLAG_REQ) {
            if ((subscriber->cbk_fn != msg->src_fn) || (subscriber->cbk_user_data != msg->src_user_data)) {
                subscriber->cbk_fn(subscriber->cbk_user_data, msg->name, &msg->value);
            }
        }
    }
}

static void metadata_rsp_forward(struct fbp_pubsub_s * self, struct message_s * msg) {
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    struct topic_s * t = topic_find_existing_base(self, msg->name);
    while (t) {
        fbp_list_foreach(&t->subscribers, item) {
            subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
            if (subscriber->flags & FBP_PUBSUB_SFLAG_RSP) {
                if ((subscriber->cbk_fn != msg->src_fn) || (subscriber->cbk_user_data != msg->src_user_data)) {
                    subscriber->cbk_fn(subscriber->cbk_user_data, msg->name, &msg->value);
                }
            }
        }
        t = t->parent;
    }
}

static void metadata_rsp_handle(struct topic_s * topic, char * topic_str) {
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;

    if (!topic || !topic->meta) {
        return;
    }
    size_t idx = strlen(topic_str);
    if ((idx == 0) || (topic_str[idx - 1] != '$')) {
        topic_str[idx] = '$';
        topic_str[idx + 1] = 0;
    }
    struct fbp_union_s meta = fbp_union_cjson_r(topic->meta);
    meta.size = (uint32_t) (strlen(topic->meta) + 1);
    while (topic) {
        fbp_list_foreach(&topic->subscribers, item) {
            subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
            if (subscriber->flags & FBP_PUBSUB_SFLAG_RSP) {
                //FBP_LOGD3("metadata_rsp_handle: %s, %s", topic_str, meta.value.str);
                subscriber->cbk_fn(subscriber->cbk_user_data, topic_str, &meta);
            }
        }
        topic = topic->parent;
    }
    topic_str[idx] = 0;
}

static void metadata_req_handle(struct topic_s * t, struct message_s * msg) {
    if (!t) {
        return;
    }
    char * topic_str = msg->name;
    size_t topic_str_len = strlen(topic_str);
    char * topic_str_last = topic_str + topic_str_len;
    struct fbp_list_s * item;
    struct topic_s * subtopic;
    fbp_list_foreach(&t->children, item) {
        subtopic = FBP_CONTAINER_OF(item, struct topic_s, item);
        topic_str_append(topic_str, subtopic->name);
        metadata_rsp_handle(subtopic, topic_str);
        metadata_req_handle(subtopic, msg);
        *topic_str_last = 0;  // reset string to original
    }
}

static void publish_meta(struct fbp_pubsub_s * self, struct message_s * msg, size_t name_sz) {
    struct topic_s * t;
    uint8_t dtype = msg->value.type;
    if (name_sz == 0) {
        FBP_LOGE("publish_meta with empty topic");
        return;
    } else if (msg->name[name_sz - 1] != '$') {
        FBP_LOGE("publish_meta, but invalid topic %s", msg->name);
        return;
    } else if (name_sz == 1) {
        // metadata request root, respond with our owned topics
        t = self->root_topic;
        msg->name[0] = 0;
        metadata_req_handle(t, msg);
        msg->name[0] = '$';
        msg->name[1] = 0;
        metadata_req_forward(self, msg);
    } else if (msg->name[name_sz - 2] == '/') {
        // metadata request topic
        msg->name[name_sz - 2] = 0;
        t = topic_find_existing_base(self, msg->name);
        if (fbp_cstr_starts_with(msg->name, self->topic_prefix)) {
            // we own this topic, fulfill metadata request
            metadata_req_handle(t, msg);
        } else {
            // not for us, forward request to link subscribers
            msg->name[name_sz - 2] = '/';
            metadata_req_forward(self, msg);
        }
    } else {
        // metadata publish
        if (fbp_cstr_starts_with(msg->name, self->topic_prefix)) {
            // for us, retain as needed
            msg->name[name_sz - 1] = 0;
            t = topic_find(self, msg->name, true);
            if (t) {
                if ((dtype == FBP_UNION_JSON)
                    && (msg->value.flags & FBP_UNION_FLAG_RETAIN)
                    && (msg->value.flags & FBP_UNION_FLAG_CONST)) {
                    t->meta = msg->value.value.str;
                    // FBP_LOGD3("metadata retain: %s, %s, %s", msg->name, t->name, t->meta);
                } else if (dtype == FBP_UNION_NULL) {
                    // query (cannot clear metadata)
                }
                metadata_rsp_handle(t, msg->name);
            }
        } else {
            // not for us, forward to response subscribers
            metadata_rsp_forward(self, msg);
        }
    }
}

static void publish_retained(struct fbp_pubsub_s * self, struct message_s * msg, size_t name_sz) {
    (void) self;
    (void) msg;
    (void) name_sz;
    // todo
}

static void publish_error(struct fbp_pubsub_s * self, struct message_s * msg, size_t name_sz) {
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    if (!name_sz || (msg->name[name_sz - 1] != '#')) {
        FBP_LOGW("invalid publish_error: %s", msg->name);
    }
    msg->name[name_sz - 1] = 0;
    struct topic_s * t = topic_find_existing_base(self, msg->name);
    msg->name[name_sz - 1] = '#';

    while (t) {
        fbp_list_foreach(&t->subscribers, item) {
            subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
            if ((subscriber->cbk_fn == msg->src_fn) && (subscriber->cbk_user_data == msg->src_user_data)) {
                continue;
            }
            if (subscriber->flags & FBP_PUBSUB_SFLAG_RSP) {
                subscriber->cbk_fn(subscriber->cbk_user_data, msg->name, &msg->value);
            }
        }
        t = t->parent;
    }
}

static uint8_t publish(struct topic_s * topic, struct message_s * msg) {
    uint8_t status = 0;
    struct fbp_list_s * item;
    struct subscriber_s * subscriber;
    while (topic) {
        fbp_list_foreach(&topic->subscribers, item) {
            subscriber = FBP_CONTAINER_OF(item, struct subscriber_s, item);
            if ((msg->src_fn == subscriber->cbk_fn) && (msg->src_user_data == subscriber->cbk_user_data)) {
                continue;
            }
            if (subscriber->flags & FBP_PUBSUB_SFLAG_NOPUB) {
                continue;
            }
            uint8_t rv = subscriber->cbk_fn(subscriber->cbk_user_data, msg->name, &msg->value);
            if (!status && rv) {
                status = rv;
            }
        }
        topic = topic->parent;
    }
    return status;
}

static void publish_normal(struct fbp_pubsub_s * self, struct message_s * msg) {
    uint8_t status = 0;
    struct topic_s * t = topic_find(self, msg->name, true);
    if (t) {
        // todo map alternate values to actual value using metadata
        if (fbp_union_eq(&t->value, &msg->value) && (t->value.flags & FBP_UNION_FLAG_RETAIN)) {
            return; // same value, skip to de-duplicate.
        }
        t->value = msg->value;
        status = publish(t, msg);
    }
    if (status) { // error
        size_t topic_sz = strlen(msg->name);
        msg->name[topic_sz] = '#';
        msg->name[topic_sz + 1] = 0;
        msg->value.type = FBP_UNION_U32;
        msg->value.flags = 0;
        msg->value.value.u32 = status;
        publish_error(self, msg, topic_sz + 1);
    }
}

static void subscribe(struct fbp_pubsub_s * self, struct message_s * msg) {
    struct topic_s * t;
    t = topic_find(self, msg->name, true);
    if (!t) {
        FBP_LOGE("could not find/create subscribe topic");
        return;
    }

    struct subscriber_s * sub = subscriber_alloc(self);
    sub->flags = (uint32_t) msg->value.value.u32;
    sub->cbk_fn = msg->src_fn;
    sub->cbk_user_data = msg->src_user_data;
    fbp_list_add_tail(&t->subscribers, &sub->item);

    if (sub->flags & FBP_PUBSUB_SFLAG_RETAIN) {
        FBP_LOGI("subscribe traverse \"%s\"", msg->name);
        subscribe_traverse(t, msg->name, sub->cbk_fn, sub->cbk_user_data);
    }
}

static void process_one(struct fbp_pubsub_s * self, struct message_s * msg) {
    uint8_t dtype = msg->value.type;
    switch (dtype) {
        case FBP_UNION_NULL: break;
        case FBP_UNION_STR: break;
        case FBP_UNION_JSON: break;
        case FBP_UNION_BIN: break;
#if FBP_CONFIG_USE_FLOAT32
         case FBP_UNION_F32: break;
#endif
#if FBP_CONFIG_USE_FLOAT64
        case FBP_UNION_F64: break;
#endif
        case FBP_UNION_U8: break;
        case FBP_UNION_U16: break;
        case FBP_UNION_U32: break;
        case FBP_UNION_U64: break;
        case FBP_UNION_I8: break;
        case FBP_UNION_I16: break;
        case FBP_UNION_I32: break;
        case FBP_UNION_I64: break;
        default:
            FBP_LOGW("unsupported type for %s: %d", msg->name, (int) msg->value.type);
            return;
    }

    size_t name_sz = strlen(msg->name);  // excluding terminator
    if (msg->value.op == OP_PUBLISH) {
        if (0 == name_sz) {
            FBP_LOGW("publish to root not allowed");
        } else {
            switch (msg->name[name_sz - 1]) {
                case '$': publish_meta(self, msg, name_sz); break;
                case '?': publish_retained(self, msg, name_sz); break;
                case '#': publish_error(self, msg, name_sz); break;
                default: publish_normal(self, msg); break;
            }
        }
    } else if (msg->value.op == OP_SUBSCRIBE) {
        subscribe(self, msg);
    } else {
        FBP_LOGW("unsupported op for %s: %d", msg->name, (int) msg->value.op);
    }
}

void fbp_pubsub_process(struct fbp_pubsub_s * self) {
    struct fbp_list_s * item;
    struct message_s * msg;
    while (1) {
        lock(self);
        item = fbp_list_remove_head(&self->msg_pend);
        unlock(self);
        if (!item) {
            return;
        }
        msg = FBP_CONTAINER_OF(item, struct message_s, item);
        process_one(self, msg);

        lock(self);
        if (is_ptr_type(msg->value.type) && (0 == (msg->value.flags & FBP_UNION_FLAG_CONST))) {
            uint32_t sz = 0;
            uint8_t * buf = fbp_rbm_pop(&self->mrb, &sz);
            if ((buf != msg->value.value.bin) || (sz != msg->value.size)) {
                FBP_LOGE("internal msgbuf sync error");
            }
        }
        fbp_list_add_tail(&self->msg_free, item);
        unlock(self);
    }
}

void fbp_pubsub_register_mutex(struct fbp_pubsub_s * self, fbp_os_mutex_t mutex) {
    self->mutex = mutex;
}
