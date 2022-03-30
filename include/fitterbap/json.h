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
 * @brief JSON parser.
 */

#ifndef FBP_JSON_H__
#define FBP_JSON_H__

#include "fitterbap/common_header.h"
#include "fitterbap/union.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_json Simple JSON parser.
 *
 * @brief Parse JSON into a token stream.
 *
 * This JSON parser is designed to support the JSON-formatted Fitterbap PubSub
 * metadata.  It operates completely from stack memory with no required
 * dynamic or static memory.  The parser is SAX-like with callbacks.
 * The parser uses fbp_union_s as the callback tokens.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The token types emitted by the parser.
 */
enum fbp_json_token_e {
    FBP_JSON_VALUE,         // dtype: string, i32, f32, null
    FBP_JSON_KEY,           // dtype: string
    FBP_JSON_OBJ_START,     // dtype: null
    FBP_JSON_OBJ_END,       // dtype: null
    FBP_JSON_ARRAY_START,   // dtype: null
    FBP_JSON_ARRAY_END,     // dtype: null
};

/**
 * @brief The function to call for each token.
 *
 * @param user_data The arbitrary user data.
 * @param token The next parsed token.  The value only remains valid for the
 *      duration of the callback.  String values are NOT null terminated, and
 *      this function must use the token->size field.
 *      The "op" field contains fbp_json_token_e.
 * @return 0 or error code to stop processing.  Use FBP_ERROR_ABORTED to signal
 *      that processing completed as expected.  All other error codes will be
 *      returned by fbp_json_parse().
 */
typedef int32_t (*fbp_json_fn)(void * user_data, const struct fbp_union_s * token);

/**
 * @brief Parse JSON into tokens.
 *
 * @param json The null-terminated JSON string to parse.
 * @param cbk_fn The function to all for each token.
 * @param cbk_user_data The arbitrary data to provide to cbk_fn.
 * @return 0 or error code.
 */
FBP_API int32_t fbp_json_parse(const char * json, fbp_json_fn cbk_fn, void * cbk_user_data);

/**
 * @brief Compare string to token.
 *
 * @param str The null-terminated string.
 * @param token The token, usually provided to the fbp_json_parse() callback.
 * @return 1 if equal, 0 if not equal.
 *
 * Since token strings are NOT null terminated, C-standard strcmp does not work.
 * memcmp works if the string lengths match, which is not guaranteed.
 */
FBP_API int32_t fbp_json_strcmp(const char * str, const struct fbp_union_s * token);


FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_JSON_H__ */
