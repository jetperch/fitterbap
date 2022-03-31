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
 * @brief PubSub topic list manipulation.
 */

#ifndef FBP_TOPIC_LIST_H__
#define FBP_TOPIC_LIST_H__

#include "fitterbap/pubsub.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_topic_list Topic list manipulation for PubSub.
 *
 * @brief Topic list string utility functions.
 *
 * @{
 */

FBP_CPP_GUARD_START

#define FBP_TOPIC_LIST_LENGTH_MAX (FBP_PUBSUB_TOPIC_LENGTH_MAX * 2)
#define FBP_TOPIC_LIST_SEP (FBP_PUBSUB_UNIT_SEP_CHR)

/// The topic list structure.
struct fbp_topic_list_s {
    /// The topic list string.
    char topic_list[FBP_TOPIC_LIST_LENGTH_MAX];
};

/// Empty topic structure initializer
#define FBP_TOPIC_LIST_INIT ((struct fbp_topic_list_s) {.topic_list={0}})

/**
 * @brief Clear a topic list structure instance to reset it to zero length.
 *
 * @param self[inout] The topic list structure, which is modified in place.
 */
FBP_API void fbp_topic_list_clear(struct fbp_topic_list_s * self);

/**
 * @brief Append a topic to the list.
 *
 * @param self[inout] The topic list.
 * @param topic The topic string to add.  Any ending '/' is omitted.
 *
 * Asserts if runs out of room.
 */
FBP_API void fbp_topic_list_append(struct fbp_topic_list_s * self, const char * topic);

/**
 * @brief Remove a topic from the list.
 *
 * @param self[inout] The topic list.
 * @param topic The topic string to remove.  Any ending '/' is ignored.
 */
FBP_API void fbp_topic_list_remove(struct fbp_topic_list_s * self, const char * topic);

/**
 * @brief Function called during list iteration.
 *
 * @param user_data The arbitrary user data.
 * @param topic The topic.
 * @return 0 to continue iteration or any other value to abort.
 */
typedef int32_t (*fbp_topic_list_cbk)(void * user_data, const char * topic);

/**
 * @brief Iterates over all topics in the list.
 *
 * @param self[inout] The topic list.
 * @param fn The function called for each topic in the list.
 * @param user_data The arbitrary data provided to fn.
 */
FBP_API void fbp_topic_list_iterate(struct fbp_topic_list_s * self, fbp_topic_list_cbk fn, void * user_data);

/**
 * @brief Search the topic list for the specified topic.
 *
 * @param self The topic list.
 * @param topic The topic to match.
 * @return true if topic found in list, false otherwise.
 */
FBP_API bool fbp_topic_list_contains(struct fbp_topic_list_s * self, const char * topic);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_TOPIC_LIST_H__ */
