/*
 * Copyright 2022 Jetperch LLC
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
 * @brief PubSub metadata handling.
 */

#ifndef FBP_PUBSUB_META_H__
#define FBP_PUBSUB_META_H__

#include "fitterbap/common_header.h"
#include "fitterbap/union.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_pubsub_meta PubSub topic metadata
 *
 * @brief Handle JSON-formatted PubSub metadata.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief Check the JSON metadata syntax.
 *
 * @param meta The JSON metadata.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_pubsub_meta_syntax_check(const char * meta);

/**
 * @brief Get the default value.
 *
 * @param meta The JSON metadata.
 * @param value[out] The parsed default value.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_pubsub_meta_default(const char * meta, struct fbp_union_s * value);

/**
 * @brief Validate a parameter value using the metadata.
 *
 * @param meta The JSON metadata.
 * @param value[inout] The value, which is modified in place.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_pubsub_meta_value(const char * meta, struct fbp_union_s * value);

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_PUBSUB_META_H__ */
