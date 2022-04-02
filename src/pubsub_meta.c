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

#include "fitterbap/pubsub_meta.h"
#include "fitterbap/ec.h"
#include "fitterbap/json.h"


struct dtype_map_s {
    const char * name;
    uint8_t dtype;
};


const struct dtype_map_s dtype_map[] = {
        {"u8", FBP_UNION_U8},
        {"u16", FBP_UNION_U16},
        {"u32", FBP_UNION_U32},
        {"u64", FBP_UNION_U64},
        {"i8", FBP_UNION_I8},
        {"i16", FBP_UNION_I16},
        {"i32", FBP_UNION_I32},
        {"i64", FBP_UNION_I64},
        {"bool", FBP_UNION_U8},
        {NULL, 0},
};


int32_t fbp_pubsub_meta_syntax_check(const char * meta) {
    // todo
    return 0;
}

enum default_state_e {
    DEFAULT_ST_DTYPE_SEARCH,
    DEFAULT_ST_DTYPE_KEY,
    DEFAULT_ST_DEFAULT_SEARCH,
    DEFAULT_ST_DEFAULT_KEY,
};

static int32_t dtype_lookup(const struct fbp_union_s * token, uint8_t * type) {
    for (const struct dtype_map_s * map = dtype_map; map->name; map++) {
        if (0 == fbp_json_strcmp(map->name, token)) {
            *type = map->dtype;
            return 0;
        }
    }
    char dtype[32];
    uint32_t sz = token->size;
    if (token->size > sizeof(dtype)) {
        sz = sizeof(dtype);
    }
    memcpy(dtype, token->value.str, sz);
    dtype[sz - 1] = 0;
    FBP_LOGE("Invalid dtype %s", dtype);
    return FBP_ERROR_PARAMETER_INVALID;
}

struct dtype_s {
    uint8_t dtype;
    uint8_t found;
};

static int32_t on_dtype(void * user_data, const struct fbp_union_s * token) {
    int32_t rc = 0;
    struct dtype_s * s = (struct dtype_s *) user_data;
    switch (token->op) {
        case FBP_JSON_VALUE:
            if (s->found) {
                rc = dtype_lookup(token, &s->dtype);
                if (!rc) {
                    rc = FBP_ERROR_ABORTED;
                }
            }
            break;
        case FBP_JSON_KEY:
            if (0 == fbp_json_strcmp("dtype", token)) {
                s->found = 1;
            }
            break;
        default: break;
    }
    return rc;
}

int32_t fbp_pubsub_meta_dtype(const char * meta, uint8_t * dtype) {
    struct dtype_s self = {0, 0};
    int32_t rc = fbp_json_parse(meta, on_dtype, &self);
    if (rc) {
        return rc;
    }
    if (!self.found) {
        return FBP_ERROR_NOT_FOUND;
    } else if (dtype) {
        *dtype = self.dtype;
    }
    return 0;
}

struct default_s {
    uint8_t state;  // default_state_e
    uint8_t depth;
    uint8_t found;
    struct fbp_union_s * value;
};

static int32_t on_default(void * user_data, const struct fbp_union_s * token) {
    int32_t rc = 0;
    struct default_s * s = (struct default_s *) user_data;
    switch (token->op) {
        case FBP_JSON_VALUE:
            if (s->state == DEFAULT_ST_DTYPE_KEY) {
                rc = dtype_lookup(token, &s->value->type);
                s->state = DEFAULT_ST_DEFAULT_SEARCH;
            } else if (s->state == DEFAULT_ST_DEFAULT_KEY) {
                s->found = 1;
                s->value->value.i64 = token->value.i64;
                rc = FBP_ERROR_ABORTED;
            }
            break;
        case FBP_JSON_KEY:
            if ((s->state == DEFAULT_ST_DTYPE_SEARCH) && (s->depth = 1) && (0 == fbp_json_strcmp("dtype", token))) {
                s->state = DEFAULT_ST_DTYPE_KEY;
            } else if ((s->state == DEFAULT_ST_DEFAULT_SEARCH) && (s->depth = 1) && (0 == fbp_json_strcmp("default", token))) {
                s->state = DEFAULT_ST_DEFAULT_KEY;
            }
            break;
        case FBP_JSON_OBJ_START: s->depth++; break;
        case FBP_JSON_OBJ_END: s->depth--; break;
        case FBP_JSON_ARRAY_START: s->depth++; break;
        case FBP_JSON_ARRAY_END: s->depth--; break;
        default: break;
    }
    return rc;
}

int32_t fbp_pubsub_meta_default(const char * meta, struct fbp_union_s * value) {
    if (!meta || !value) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    value->type = FBP_UNION_NULL;
    value->flags = 0;
    value->op = 0;
    value->app = 0;
    value->size = 0;
    value->value.u64 = 0;

    struct default_s self = {
            .state = DEFAULT_ST_DTYPE_SEARCH,
            .depth = 0,
            .found = 0,
            .value=value
    };
    int32_t rc = fbp_json_parse(meta, on_default, &self);
    if (rc) {
        return rc;
    }
    if (!self.found) {
        value->type = FBP_UNION_NULL;
    }
    return 0;
}

enum value_state_e {
    VALUE_ST_DTYPE_SEARCH,
    VALUE_ST_DTYPE_KEY,
    VALUE_ST_SEARCH,
    VALUE_ST_RANGE_KEY,
    VALUE_ST_RANGE_VAL,
    VALUE_ST_OPTIONS,
    VALUE_ST_OPTIONS_VAL,
    VALUE_ST_OPTIONS_MATCH,
};

struct value_s {
    uint8_t state;  // value_state_e
    uint8_t type;
    uint8_t depth;
    uint8_t array_idx;
    struct fbp_union_s * value;
    union fbp_union_inner_u option;
    union fbp_union_inner_u range[3];
};

static int32_t on_value(void * user_data, const struct fbp_union_s * token) {
    int32_t rc = 0;
    struct value_s * s = (struct value_s *) user_data;
    struct fbp_union_s t;
    switch (token->op) {
        case FBP_JSON_VALUE:
            if (s->state == VALUE_ST_DTYPE_KEY) {
                rc = dtype_lookup(token, &s->type);
                s->state = VALUE_ST_SEARCH;
            } else if (s->state == VALUE_ST_RANGE_VAL) {
                s->range[s->array_idx++].u64 = token->value.u64;
            } else if (s->state == VALUE_ST_OPTIONS_VAL) {
                if (0 == s->array_idx++) {
                    t = *token;
                    if (fbp_union_as_type(&t, s->type)) {
                        return FBP_ERROR_PARAMETER_INVALID;
                    }
                    s->option.u64 = t.value.u64;
                }
                if (fbp_union_equiv(s->value, token)) {
                    s->value->value.u64 = s->option.u64;
                    s->value->type = s->type;
                    s->state = VALUE_ST_OPTIONS_MATCH;
                }
            }
            break;
        case FBP_JSON_KEY:
            if ((s->state == VALUE_ST_DTYPE_SEARCH) && (s->depth = 1) && (0 == fbp_json_strcmp("dtype", token))) {
                s->state = VALUE_ST_DTYPE_KEY;
            } else if ((s->state == VALUE_ST_SEARCH) && (s->depth = 1) && (0 == fbp_json_strcmp("range", token))) {
                s->state = VALUE_ST_RANGE_KEY;
            } else if ((s->state == VALUE_ST_SEARCH) && (s->depth = 1) && (0 == fbp_json_strcmp("options", token))) {
                s->state = VALUE_ST_OPTIONS;
            }
            break;
        case FBP_JSON_OBJ_START: s->depth++; break;
        case FBP_JSON_OBJ_END: s->depth--; break;
        case FBP_JSON_ARRAY_START:
            s->depth++;
            if ((s->state == VALUE_ST_OPTIONS) && (s->depth == 3)) {
                s->array_idx = 0;
                s->state = VALUE_ST_OPTIONS_VAL;
            }
            if (s->state == VALUE_ST_RANGE_KEY) {
                s->array_idx = 0;
                s->state = VALUE_ST_RANGE_VAL;
            }
            break;
        case FBP_JSON_ARRAY_END:
            if ((s->state == VALUE_ST_OPTIONS_VAL) && (s->depth == 3)) {
                s->state = VALUE_ST_OPTIONS;
            } else if ((s->state == VALUE_ST_OPTIONS) && (s->depth == 2)) {
                rc = FBP_ERROR_PARAMETER_INVALID;  // no match
            } else if ((s->state == VALUE_ST_OPTIONS_MATCH) && (s->depth == 2)) {
                s->state = VALUE_ST_SEARCH;
            } else if ((s->state == VALUE_ST_RANGE_VAL) && (s->depth == 2)) {
                s->state = VALUE_ST_SEARCH;
            }
            s->depth--;
        break;
        default: break;
    }
    return rc;
}

int32_t fbp_pubsub_meta_value(const char * meta, struct fbp_union_s * value) {
    if (!meta || !value) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    struct value_s self = {
            .state = VALUE_ST_DTYPE_SEARCH,
            .type = FBP_UNION_NULL,
            .depth = 0,
            .value=value,
            .option={.u64=0},
            .range={{.u64=0}, {.u64=0}, {.u64=0}}
    };
    return fbp_json_parse(meta, on_value, &self);
}
