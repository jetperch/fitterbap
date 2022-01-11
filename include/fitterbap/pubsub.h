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


/**
 * @file
 *
 * @brief Trivial publish-subscribe.
 */

#ifndef FBP_PUBSUB_H__
#define FBP_PUBSUB_H__

#include "fitterbap/common_header.h"
#include "fitterbap/os/mutex.h"
#include "fitterbap/union.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_pubsub Publish-Subscribe
 *
 * @brief A simple, opinionated, distributed Publish-Subscribe architecture.
 *
 *
 * @{
 */

FBP_CPP_GUARD_START

/// The maximum topic length, include indicator char and null terminator.
#ifndef FBP_PUBSUB_TOPIC_LENGTH_MAX
#define FBP_PUBSUB_TOPIC_LENGTH_MAX (32)
#endif

/// The maximum topic length between '/' separators.
#define FBP_PUBSUB_TOPIC_LENGTH_PER_LEVEL (8)
/// The unit separator character
#define FBP_PUBSUB_UNIT_SEP_CHR ((char) 0x1f)
/// The unit separator as a single character string
#define FBP_PUBSUB_UNIT_SEP_STR "\x1f"

// Well-known topics
#define FBP_PUBSUB_TOPIC_PREFIX "_/topic/prefix"
#define FBP_PUBSUB_TOPIC_LIST "_/topic/list"
#define FBP_PUBSUB_TOPIC_ADD "_/topic/add"
#define FBP_PUBSUB_TOPIC_REMOVE "_/topic/remove"
#define FBP_PUBSUB_CONN_ADD "./conn/add"            // list of topics
#define FBP_PUBSUB_CONN_REMOVE "./conn/remove"      // list of topics


/// The subscriber flags for fbp_pubsub_subscribe().
enum fbp_pubsub_sflag_e {
    /// No flags (always 0).
    FBP_PUBSUB_SFLAG_NONE = 0,
    /// Send retained messages to subscriber as soon as possible.
    FBP_PUBSUB_SFLAG_RETAIN = (1 << 0),
    /// Do not receive normal topic publish.
    FBP_PUBSUB_SFLAG_NOPUB = (1 << 1),
    /// Subscribe to receive metadata requests and queries.
    FBP_PUBSUB_SFLAG_REQ = (1 << 2),
    /// Subscribe to receive metadata responses and query responses.
    FBP_PUBSUB_SFLAG_RSP = (1 << 3),
};

/// The opaque PubSub instance.
struct fbp_pubsub_s;

/**
 * @brief Function called on topic updates.
 *
 * @param user_data The arbitrary user data.
 * @param topic The topic for this update.
 * @param value The value for this update.
 * @return 0 or error code.  Only the topic "owner" should response
 *      with an error code.
 *
 * If this callback responds with an error code, then the pubsub
 * instance will publish a topic# error.
 */
typedef uint8_t (*fbp_pubsub_subscribe_fn)(void * user_data,
        const char * topic, const struct fbp_union_s * value);

/**
 * @brief Function called whenever a new message is published.
 *
 * @param user_data Arbitrary user data.
 *
 * When used in a threaded environment, this function can signal the
 * thread that it should call fbp_pubsub_process().  This automatic
 * hook often eliminates the need for more complicated wrappers.
 */
typedef void (*fbp_pubsub_on_publish_fn)(void * user_data);

/**
 * @brief Publish to a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to update.
 * @param value The new value for the topic.
 * @param src_fn The callback function for the source subscriber
 *      that is publishing the update.  Can be NULL.
 * @param src_user_data The arbitrary user data for the source subscriber
 *      callback function.
 * @return 0 or error code.
 * @see fbp_pubsub_publish()
 */
typedef int32_t (*fbp_pubsub_publish_fn)(
        struct fbp_pubsub_s * self,
        const char * topic, const struct fbp_union_s * value,
        fbp_pubsub_subscribe_fn src_fn, void * src_user_data);

/**
 * @brief Create and initialize a new PubSub instance.
 *
 * @param topic_prefix The topic prefix that is owned by this
 *      pubsub instance.  This instance will reply to metadata and
 *      query requests for all topics starting with this prefix.
 * @param buffer_size The buffer size for dynamic pointer messages.
 *      0 prohibits non-CONST pointer types.
 * @return The new PubSub instance.
 */
FBP_API struct fbp_pubsub_s * fbp_pubsub_initialize(const char * topic_prefix, uint32_t buffer_size);

/**
 * @brief Finalize the instance and free resources.
 *
 * @param self The PubSub instance.
 */
FBP_API void fbp_pubsub_finalize(struct fbp_pubsub_s * self);

/**
 * @brief Get the topic prefix that is owned by this instance.
 *
 * @param self The PubSub instance.
 * @return The topic prefix provided to fbp_pubsub_initialize().
 */
FBP_API const char * fbp_pubsub_topic_prefix(struct fbp_pubsub_s * self);

/**
 * @brief Register the function called for each call to fbp_pubsub_publish().
 *
 * @param self The PubSub instance.
 * @param cbk_fn The callback function.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 *
 * Threaded implementations can use this callback to set an event,
 * task notification, or file handle to tell the thread that
 * fbp_pubsub_process() should be invoked.
 */
FBP_API void fbp_pubsub_register_on_publish(struct fbp_pubsub_s * self,
        fbp_pubsub_on_publish_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Subscribe to a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to subscribe.
 * @param flags The fbp_pubsub_sflag_e flags
 * @param cbk_fn The function to call on topic updates.
 *      Invocations are from fbp_pubsub_process().
 *      The cbk_fn is responsible for any thread resynchronization.
 * @param cbk_user_data The arbitrary data for cbk_fn.
 * @return 0 or error code.
 * @see fbp_pubsub_unsubscribe()
 *
 * If the topic does not already exist, this function will
 * automatically create it.
 *
 * Note that the flags are critical to implementing the distributed
 * architecture.  The system constructs the polytree architecture with
 * LINK topic subscriptions.
 * The server PubSub instance subscribes to all topics in client PubSub
 * instances.  Client PubSub instances subscribe to their topic_prefix along
 * with all topic_prefix for any PubSub instances for which they are a server.
 */
FBP_API int32_t fbp_pubsub_subscribe(struct fbp_pubsub_s * self, const char * topic,
        uint8_t flags,
        fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Unsubscribe from a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to unsubscribe.
 * @param cbk_fn The function provided to fbp_pubsub_subscribe().
 * @param cbk_user_data The arbitrary data provided to fbp_pubsub_subscribe().
 * @return 0 or error code.
 * @see fbp_pubsub_subscribe()
 *
 * Unlike other pubsub calls like fbp_pubsub_subscribe() and
 * fbp_pubsub_publish(), this function runs in the caller's context.
 * While cbk_fn may be called within this function, it is guaranteed to
 * not be called once the function returns.
 */
FBP_API int32_t fbp_pubsub_unsubscribe(struct fbp_pubsub_s * self, const char * topic,
        fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Unsubscribe from all topics.
 *
 * @param self The PubSub instance.
 * @param cbk_fn The function provided to fbp_pubsub_subscribe().
 * @param cbk_user_data The arbitrary data provided to fbp_pubsub_subscribe().
 * @return 0 or error code.
 * @see fbp_pubsub_subscribe()
 * @see fbp_pubsub_unsubscribe()
 *
 * Unlike other pubsub calls like fbp_pubsub_subscribe() and
 * fbp_pubsub_publish(), this function runs in the caller's context.
 * While cbk_fn may be called within this function, it is guaranteed to
 * not be called once the function returns.
 */
FBP_API int32_t fbp_pubsub_unsubscribe_from_all(struct fbp_pubsub_s * self,
        fbp_pubsub_subscribe_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Publish to a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic to update.
 * @param value The new value for the topic.
 * @param src_fn The callback function for the source subscriber
 *      that is publishing the update.  Can be NULL.
 * @param src_user_data The arbitrary user data for the source subscriber
 *      callback function.
 * @return 0 or error code.
 * @see fbp_pubsub_publish_fn()
 *
 * If the topic does not already exist, this function will
 * automatically create it.
 *
 * The src_fn and src_user_data provide trivial, built-in support
 * to ensure that a publisher/subscriber does not receive their
 * own updates.  One deduplication technique is for each
 * subscriber to compare values. However, some PubSub instances,
 * such as communication bridges between PubSub instances,
 * would need to duplicate the full PubSub state.
 * This approach greatly simplifies implementing a
 * subscriber that also publishes to the same topics.
 *
 * This modules supports two types of pointer types.  Values marked with
 * the FBP_UNION_FLAG_CONST remain owned by the caller.  The values
 * must remain valid until the pubsub instance completes publishing.
 * If also marked with FBP_UNION_FLAG_RETAIN, the value must remain
 * valid until a new value publishes.  We only recommend using this
 * method with "static const" values.  Note that properly freeing a
 * pointer type is not trivial, since publishing is asynchronous and
 * subscriber calling order is not guaranteed.
 *
 * One "trick" to freeing pointers is to publish two messages:
 * first one with the pointer and then one with NULL.
 * If the publisher also subscribes, then they can free the pointer
 * when they receive the NULL value.  However, this requires that
 * all subscribers only operated on the pointer during the subscriber
 * callback and do not hold on to it.
 *
 * The second pointer type is dynamically managed by the pubsub instance.
 * FBP_UNION_FLAG_RETAIN is NOT allowed for these pointer types as
 * they are only temporarily allocated in a circular buffer.
 * If the item is too big to ever fit, this function returns
 * FBP_ERROR_PARAMETER_INVALID.
 * If the circular buffer is full, this function returns
 * FBP_ERROR_NOT_ENOUGH_MEMORY.  The caller can optionally wait and retry.
 */
FBP_API int32_t fbp_pubsub_publish(struct fbp_pubsub_s * self,
        const char * topic, const struct fbp_union_s * value,
        fbp_pubsub_subscribe_fn src_fn, void * src_user_data);

/**
 * @brief Convenience function to set the topic metadata.
 *
 * @param self The PubSub instance.
 * @param topic The topic name.
 * @param meta_json The JSON-formatted UTF-8 metadata string for the topic.
 * @return 0 or error code.
 * @see pubsub.md for the metadata format definition.
 *
 * Although you can use fbp_pubsub_publish() with topic + '$' and
 * a const, retained JSON string, this function simplifies the
 * metadata call.
 */
FBP_API int32_t fbp_pubsub_meta(struct fbp_pubsub_s * self, const char * topic, const char * meta_json);

/**
 * @brief Get the local, retained value for a topic.
 *
 * @param self The PubSub instance.
 * @param topic The topic name.
 * @param[out] value The current value for topic.  Since this request is
 *      handled in the caller's thread, it does not account
 *      for any updates queued for fbp_pubsub_process().
 * @return 0 or error code.
 *
 * For a distributed PubSub implementation, use topic? to get the retained
 * value directly from the owning PubSub instance.
 */
FBP_API int32_t fbp_pubsub_query(struct fbp_pubsub_s * self, const char * topic, struct fbp_union_s * value);

/**
 * @brief Process all outstanding topic updates.
 *
 * @param self The PubSub instance to process.
 *
 * Many implementation choose to run this from a unique thread.
 */
FBP_API void fbp_pubsub_process(struct fbp_pubsub_s * self);

/**
 * @brief Register functions to lock and unlock the send-side mutex.
 *
 * @param self The instance.
 * @param mutex The mutex instance for accessing self.
 *
 * Registering a mutex allows the implementation to be reentrant for
 * multi-threaded operation.
 */
FBP_API void fbp_pubsub_register_mutex(struct fbp_pubsub_s * self, fbp_os_mutex_t mutex);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_PUBSUB_H__ */
