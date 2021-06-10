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

#include "fitterbap/topic.h"
#include "fitterbap/ec.h"
#include "fitterbap/platform.h"


void fbp_topic_append(struct fbp_topic_s * topic, const char * subtopic) {
    char * topic_end = &topic->topic[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    if (topic->length && ('/' != topic->topic[topic->length -1])) {
        topic->topic[topic->length++] = '/';
    }

    char * t = &topic->topic[topic->length];
    while (*subtopic && (t < topic_end)) {
        char c = *subtopic++;
        *t++ = c;
    }
    FBP_ASSERT(t < topic_end);
    *t = 0;
    topic->length = t - topic->topic;
}

void fbp_topic_set(struct fbp_topic_s * topic, const char * str) {
    int32_t rc = 0;
    fbp_topic_reset(topic);
    while (*str && (topic->length < FBP_PUBSUB_TOPIC_LENGTH_MAX)) {
        topic->topic[topic->length++] = *str++;
    }
    FBP_ASSERT(topic->length < FBP_PUBSUB_TOPIC_LENGTH_MAX);
    topic->topic[topic->length] = 0;
}

void fbp_topic_append_char(struct fbp_topic_s * topic, char ch) {
    FBP_ASSERT(topic->length < (FBP_PUBSUB_TOPIC_LENGTH_MAX - 1));
    topic->topic[topic->length++] = ch;
    topic->topic[topic->length] = 0;
}
