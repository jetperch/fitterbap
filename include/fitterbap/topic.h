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


/**
 * @file
 *
 * @brief PubSub topic manipulation.
 */

#ifndef FBP_TOPIC_H__
#define FBP_TOPIC_H__

#include "fitterbap/pubsub.h"
#include <stdint.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_topic Topic manipulation for PubSub.
 *
 * @brief Topic string utility functions.
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The topic structure.
struct fbp_topic_s {
    /// The topic string.
    char topic[FBP_PUBSUB_TOPIC_LENGTH_MAX];
    /// The length in bytes ignoring the null terminator.
    uint8_t length;
};

/// Empty topic structure initializer
#define FBP_TOPIC_INIT ((struct fbp_topic_s) {.topic={0}, .length=0})

/**
 * @brief Clear a topic structure instance to reset it to zero length.
 *
 * @param topic[inout] The topic structure, which is modified in place.
 */
static inline void fbp_topic_clear(struct fbp_topic_s * topic) {
    topic->topic[0] = 0;
    topic->length = 0;
}

/**
 * @brief Truncate a topic structure to a specified length.
 *
 * @param topic[inout] The topic structure, which is modified in place.
 * @param length The desired length.
 *
 * If you store length before calling fbp_topic_append(), you can
 * use this function to revert the append.
 */
static inline void fbp_topic_truncate(struct fbp_topic_s * topic, uint8_t length) {
    if (length < topic->length) {
        topic->topic[length] = 0;
        topic->length = length;
    }
}

/**
 * @brief Append a subtopic to a topic structure.
 *
 * @param topic[inout] The topic structure, which is modified in place.
 * @param subtopic The subtopic string to add.
 * @see
 *
 * This function intelligently adds the '/' separator.  If the topic
 * does not already end with '/', it will be inserted first.
 */
void fbp_topic_append(struct fbp_topic_s * topic, const char * subtopic);

/**
 * @brief Set the topic to the provided value.
 *
 * @param topic[inout] The topic structure, which is modified in place.
 * @param str The desired topic value.
 *
 * This function will assert if no room remains.
 */
void fbp_topic_set(struct fbp_topic_s * topic, const char * str);

/**
 * @brief Appends a character to the topic.
 *
 * @param topic[inout] The topic structure, which is modified in place.
 * @param ch The special character to append.
 *
 * This function will assert if no room remains.
 */
void fbp_topic_append_char(struct fbp_topic_s * topic, char ch);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_TOPIC_H__ */
