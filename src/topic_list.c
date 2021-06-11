/*
 * Copyright 2021 Jetperch LLC
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

#include "fitterbap/topic_list.h"
#include "fitterbap/ec.h"
#include "fitterbap/platform.h"
#include "fitterbap/assert.h"
#include <string.h>


void fbp_topic_list_clear(struct fbp_topic_list_s * self) {
    if (self) {
        self->topic_list[0] = 0;
    }
}

void fbp_topic_list_append(struct fbp_topic_list_s * self, const char * topic) {
    if (!self || !topic || !*topic) {
        return;
    }
    fbp_topic_list_remove(self, topic); // prevent duplicates
    size_t sz_orig = strlen(self->topic_list);
    size_t sz_topic = strlen(topic);
    if (topic[sz_topic - 1] == '/') {
        --sz_topic;
        if (!sz_topic) {
            return;
        }
    }

    FBP_ASSERT((sz_orig + sz_topic + 2) <= FBP_TOPIC_LIST_LENGTH_MAX);
    char * t = self->topic_list + sz_orig;
    if (sz_orig) {
        *t++ = FBP_TOPIC_LIST_SEP;
    }
    for (size_t i = 0; i < sz_topic; ++i) {
        *t++ = topic[i];
    }
    *t = 0;
}

void fbp_topic_list_remove(struct fbp_topic_list_s * self, const char * topic) {
    char * dst = self->topic_list;
    const char * r = topic;
    char * s = dst;  // start of current matching section
    char * c = dst;  // current matching character
    char * w = dst;  // output

    while (1) {
        if ((!*r) && ((!*c) || (*c == FBP_PUBSUB_UNIT_SEP_CHR))) {
            // match, do not copy
            if (!*c) {
                break;
            }
            r = topic;
            ++c;
            s = c;
        } else if (*r == *c) {  // still matching
            ++r;
            ++c;
        } else {  // not matching, advance
            if (w != dst) {
                *w++ = FBP_PUBSUB_UNIT_SEP_CHR;
            }
            while (*s && (*s != FBP_PUBSUB_UNIT_SEP_CHR)) {
                *w++ = *s++;
            }
            if (!*s) {
                break;
            }
            r = topic;
            ++s;
            c = s;
        }
    }
    *w = 0; // null terminate output
}

void fbp_topic_list_iterate(struct fbp_topic_list_s * self, fbp_topic_list_cbk fn, void * user_data) {
    if (!self || !self->topic_list[0] || !fn) {
        return;
    }
    char * s = self->topic_list;
    char * c = s;
    char sep;
    while (1) {
        if (!*c || (*c == FBP_TOPIC_LIST_SEP)) {
            sep = *c;
            *c = 0;
            fn(user_data, s);
            if (!sep) {
                break;
            }
            *c++ = sep;
            s = c;
        } else {
            c++;
        }
    }
}
