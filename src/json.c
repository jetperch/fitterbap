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

#include "fitterbap/json.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"

#define delim(op__) ((struct fbp_union_s){.type=FBP_UNION_NULL, .op=op__, .flags=0, .app=0, .value={.u64=0}, .size=0})
const char * WHITESPACE = " \n\t\r";
const char * ESCAPE = "\"\\/bfnrtu";

#define NEXT(s__)       (s__)->json[(s__)->offset]
#define ADVANCE(s__)    (s__)->offset++
#define ROE(x)   do {int32_t rc__ = (x); if (rc__) {return rc__;}} while (0)

struct state_s {
    const char * json;
    uint32_t offset;

    fbp_json_fn cbk_fn;
    void * cbk_user_data;
};

static int32_t parse_value(struct state_s * s);

static int32_t on_token_ignore(void * user_data, const struct fbp_union_s * token) {
    (void) user_data;
    (void) token;
    return 0;
}

static int32_t is_in(char ch, const char * list) {
    for (const char * w = list; *w; ++w) {
        if (ch == *w) {
            return 1;
        }
    }
    return 0;
}

static int32_t is_whitespace(char ch) { return is_in(ch, WHITESPACE); }
static int32_t is_escape(char ch) { return is_in(ch, ESCAPE); }

static int32_t to_hex(char v) {
    if ((v >= '0') && (v <= '9')) {
        return v - '0';
    } else if ((v >= 'a') && (v <= 'f')) {
        return v - 'a' + 10;
    } else if ((v >= 'A') && (v <= 'F')) {
        return v - 'A' + 10;
    } else {
        return -1;
    }
}

static void skip_whitespace(struct state_s * s) {
    while (is_whitespace(NEXT(s))) {
        ADVANCE(s);
    }
}

#define emit(state__, token__) {                                        \
    struct state_s * state_ = (state__);                                \
    int32_t rc__ = state_->cbk_fn(state_->cbk_user_data, token__);      \
    if (rc__) {                                                         \
        return rc__;                                                    \
    }                                                                   \
}

static int32_t parse_string(struct state_s * s, uint8_t op) {
    if (NEXT(s) != '"') {
        return FBP_ERROR_SYNTAX_ERROR;
    }
    ADVANCE(s);
    uint32_t offset_start = s->offset;
    char ch;
    while (1) {
        ch = NEXT(s);
        if (!ch) {
            FBP_LOGW("unterminated string starting at %u", offset_start - 1);
            return FBP_ERROR_SYNTAX_ERROR;
        }
        if (ch == '"') {
            break;
        } else if (ch == '\\') {
            ADVANCE(s);
            ch = NEXT(s);
            if (!is_escape(ch)) {
                FBP_LOGW("invalid string escape %c at %u", ch, s->offset);
                return FBP_ERROR_SYNTAX_ERROR;
            }
            if (ch == 'u') {
                for (int i = 0; i < 4; i++) {
                    ADVANCE(s);
                    ch = NEXT(s);
                    if (to_hex(ch) < 0) {
                        FBP_LOGW("invalid string escape hex %c at %u", ch, s->offset);
                        return FBP_ERROR_SYNTAX_ERROR;
                    }
                }
            }
        }
        ADVANCE(s);
    }
    uint32_t sz = s->offset - offset_start;
    struct fbp_union_s value = {.type=FBP_UNION_STR, .op=op, .flags=FBP_UNION_FLAG_CONST, .app=0, .value={.str=&s->json[offset_start]}, .size=sz};
    emit(s, &value);
    s->offset++;
    skip_whitespace(s);
    return 0;
}

static int32_t parse_object(struct state_s * s) {
    if (NEXT(s) != '{') {
        return FBP_ERROR_SYNTAX_ERROR;
    }
    emit(s, &delim(FBP_JSON_OBJ_START));
    ADVANCE(s);
    skip_whitespace(s);
    while (NEXT(s) != '}') {
        skip_whitespace(s);
        ROE(parse_string(s, FBP_JSON_KEY));
        skip_whitespace(s);
        if (NEXT(s) != ':') {
            FBP_LOGE("byte %u: expect object separator", s->offset);
            return FBP_ERROR_SYNTAX_ERROR;
        }
        ADVANCE(s);
        ROE(parse_value(s));
        skip_whitespace(s);
        if (NEXT(s) == ',') {
            ADVANCE(s);
            if (NEXT(s) == '}') {
                FBP_LOGE("byte %u: trailing comma", s->offset);
                return FBP_ERROR_SYNTAX_ERROR;
            }
        }
    }
    emit(s, &delim(FBP_JSON_OBJ_END));
    ADVANCE(s);
    return 0;
}

static int32_t parse_array(struct state_s * s) {
    if (NEXT(s) != '[') {
        return FBP_ERROR_SYNTAX_ERROR;
    }
    emit(s, &delim(FBP_JSON_ARRAY_START));
    ADVANCE(s);
    skip_whitespace(s);
    while (NEXT(s) != ']') {
        skip_whitespace(s);
        ROE(parse_value(s));
        skip_whitespace(s);
        if (NEXT(s) == ',') {
            ADVANCE(s);
            skip_whitespace(s);
            if (NEXT(s) == ']') {
                FBP_LOGE("byte %u: trailing comma", s->offset);
                return FBP_ERROR_SYNTAX_ERROR;
            }
        }
    }
    emit(s, &delim(FBP_JSON_ARRAY_END));
    ADVANCE(s);
    return 0;
}

static int32_t parse_literal(struct state_s * s, const char * literal, struct fbp_union_s * value) {
    uint32_t offset = s->offset;
    char ch;
    while (*literal) {
        ch = NEXT(s);
        if (!ch) {
            FBP_LOGE("byte %u: invalid value", offset);
            return FBP_ERROR_SYNTAX_ERROR;
        }
        literal++;
        ADVANCE(s);
    }
    emit(s, value);
    return 0;
}

static int32_t parse_number(struct state_s * s) {
    bool is_neg = false;
    int32_t whole = 0;
    uint32_t offset = s->offset;
    char ch;
    if (NEXT(s) == '-') {
        is_neg = true;
        ADVANCE(s);
    }

    ch = NEXT(s);
    if (ch == '0') {
        ADVANCE(s);
    } else if ((ch >= '1') && (ch <= '9')) {
        while (1) {
            ch = NEXT(s);
            if ((ch >= '0') && (ch <= '9')) {
                whole = whole * 10 + (ch - '0');
            } else {
                break;
            }
            ADVANCE(s);
        }
    } else {
        FBP_LOGE("byte %u: invalid value", offset);
        return FBP_ERROR_SYNTAX_ERROR;
    }

    if (!is_in(NEXT(s), ".eE")) {  // i32
        if (is_neg) {
            whole = -whole;
        }
        emit(s, &fbp_union_i32(whole));
    } else { // f32 support
        FBP_LOGE("byte %u: floating point numbers not yet supported", offset);
        return FBP_ERROR_NOT_SUPPORTED;  // todo f32 support
        /*
        if (NEXT(s) == '.') {
            // fraction
        }
        ch = NEXT(s);
        if ((ch == 'e') || (ch == 'E')) {
            // exponent
        }
        */
    }

    return 0;
}

static int32_t parse_value(struct state_s * s) {
    skip_whitespace(s);
    switch (NEXT(s)) {
        case 0:
            FBP_LOGE("byte %u: end of json, but expected value", s->offset);
            return FBP_ERROR_SYNTAX_ERROR;
        case '{': return parse_object(s);
        case '[': return parse_array(s);
        case '"': return parse_string(s, FBP_JSON_VALUE);
        case 't': return parse_literal(s, "true", &fbp_union_i32(1));
        case 'f': return parse_literal(s, "false", &fbp_union_i32(0));
        case 'n': return parse_literal(s, "null", &fbp_union_null());
        default: return parse_number(s);
    }
}

int32_t fbp_json_parse(const char * json, fbp_json_fn cbk_fn, void * cbk_user_data) {
    int32_t rc;
    if (!json) {
        return FBP_ERROR_PARAMETER_INVALID;
    }

    struct state_s s = {
            .json = json,
            .offset = 0,
            .cbk_fn = cbk_fn,
            .cbk_user_data = cbk_user_data,
    };
    if (!cbk_fn) {
        s.cbk_fn = on_token_ignore;
    }
    rc = parse_value(&s);
    return (FBP_ERROR_ABORTED == rc) ? 0 : rc;
}

int32_t fbp_json_strcmp(const char * str, const struct fbp_union_s * token) {
    if (!str) {
        return -2;
    }
    if (!token || (token->type != FBP_UNION_STR)) {
        return 2;
    }
    for (uint32_t i = 0; i < token->size; ++i) {
        if (*str != token->value.str[i]) {
            if (!*str || (*str < token->value.str[i])) {
                return -1;
            } else {
                return 1;
            }
        }
        ++str;
    }
    if (*str) {
        return 1;
    }
    return 0;
}
