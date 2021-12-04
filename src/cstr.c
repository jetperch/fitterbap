/*
 * Copyright 2015-2021 Jetperch LLC
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

#include "fitterbap/cstr.h"
#include "fitterbap/ec.h"


#define FLOAT_EXP_MAX (38)


static char _toupper(char c) {
    if ((c >= 'a') && c <= 'z') {
        c = c - 'a' + 'A';
    }
    return c;
}

static int _isspace(char c) {
    if ((c == ' ') || ((c >= 9) && (c <= 13))) {
        return 1;
    }
    return 0;
}

static inline int _isdigit(char c) {
    return ((c >= '0') && (c <= '9'));
}


int fbp_cstr_copy(char * tgt, char const * src, fbp_size_t tgt_size) {
    if ((NULL == tgt) || (tgt_size <= 0)) {
        return -1; // nonsensical input.
    }
    if (NULL == src) {
        *tgt = 0;
        return 0;
    }
    char * tgt_end = tgt + tgt_size - 1;
    while (1) {
        if (!*src) {
            break;
        }
        if (tgt >= tgt_end) {
            *tgt = 0;
            return 1;
        }
        *tgt++ = *src++;
    }
    *tgt = 0; // ensure NULL terminated.
    return 0;
}

int fbp_cstr_join(char * tgt, char const * src1, char const * src2, fbp_size_t tgt_size) {
    if ((NULL == tgt) || (tgt_size <= 0)) {
        return -1; // nonsensical input.
    }
    if (tgt == src2) {
        return 2;
    }
    char * tgt_end = tgt + tgt_size - 1;
    while (src1 && *src1) {
        *tgt++ = *src1++;
        if (tgt >= tgt_end) {
            *tgt = 0;
            return 1;
        }
    }
    while (src2 && *src2) {
        *tgt++ = *src2++;
        if (tgt >= tgt_end) {
            *tgt = 0;
            return 1;
        }
    }
    *tgt = 0;
    return 0;
}

int fbp_cstr_casecmp(const char * s1, const char * s2) {
    char c1;
    char c2;
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }
    while (1) {
        if (*s1 == 0) {
            if (*s2 == 0) {
                return 0;
            }
            return -1;
        } else if (*s2 == 0) {
            return 1;
        }
        c1 = _toupper(*s1);
        c2 = _toupper(*s2);
        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
        ++s1;
        ++s2;
    }
}

const char * fbp_cstr_starts_with(const char * s, const char * prefix) {
    if (!prefix || !s) {
        return s;
    }
    while (1) {
        if (*prefix == 0) {
            return s;
        }
        if ((*s == 0) || (*prefix != *s)) {
            return 0;
        }
        ++prefix;
        ++s;
    }
}

int fbp_cstr_to_u32(const char * src, uint32_t * value) {
    uint32_t v = 0;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && _isspace((uint8_t) *src)) {
        ++src;
    }
    if (!*src) { // empty string.
        return 1;
    }
    if ((src[0] == '0') && (src[1] == 'x')) {
        // hex string
        uint32_t nibble;
        src += 2;
        while (*src) {
            char c = *src;
            if ((c >= '0') && (c <= '9')) {
                nibble = c - '0';
            } else if ((c >= 'a') && (c <= 'f')) {
                nibble = c - 'a' + 10;
            } else if ((c >= 'A') && (c <= 'F')) {
                nibble = c - 'A' + 10;
            } else {
                break;
            }
            v = v * 16 + nibble;
            ++src;
        }
    } else {
        while ((*src >= '0') && (*src <= '9')) {
            v = v * 10 + (*src - '0');
            ++src;
        }
    }
    while (*src) {
        if (!_isspace((uint8_t) *src++)) { // did not parse full string
            return 1;
        }
    }
    *value = v;
    return 0;
}

int fbp_cstr_to_i32(const char * src, int32_t * value) {
    int neg = 0;
    uint32_t v;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && _isspace((uint8_t) *src)) {
        ++src;
    }

    if (*src == '-') {
        neg = 1;
        ++src;
    } else if (*src == '+') {
        ++src;
    }

    int rc = fbp_cstr_to_u32(src, &v);
    if (rc) {
        return rc;
    }
    *value = (int32_t) v;
    if (neg) {
        *value = -*value;
    }
    return 0;
}

int fbp_cstr_to_i32s(const char * src, int32_t exponent, int32_t * value) {
    int32_t v = 0;
    int32_t decimal = -1;  // still on integer part
    int neg = 0;

    if (!src) {
        return 0;
    }
    if (exponent < 0) {
        return -1;
    }

    while (*src && _isspace((uint8_t) *src)) {
        ++src;
    }

    if (*src == '-') {
        neg = 1;
        ++src;
    } else if (*src == '+') {
        ++src;
    }

    while (decimal < exponent) {
        if (*src == '.') {
            decimal = 0;
            ++src;
            continue;
        }
        if ((*src == 0) || (*src == ',')) {
            if (decimal < 0) {
                break;
            } else {
                v *= 10;
                ++decimal;
            }
        } else {
            if (decimal >= 0) {
                ++decimal;
            }
            v *= 10;
            if ((*src >= '0') && (*src <= '9')) {
                v += *src - '0';
            } else {
                return 1;
            }
            ++src;
        }
    }

    while (*src) {
        if ((*src >= '0') && (*src <= '9')) {
            // discard extra digits
        } else if (!_isspace((uint8_t) *src)) { // did not parse full string
            return 1;
        }
        ++src;
    }

    if (decimal < 0) {
        decimal = 0;
    }
    while (decimal < exponent) {
        v *= 10;
        ++decimal;
    }
    if (neg) {
        v = -v;
    }
    if (value) {
        *value = v;
    }
    return 0;
}

#if FBP_CONFIG_USE_CSTR_FLOAT
const float exp_pow[] = {  // binary lookup table for exponent
        10.0e1f,
        10.0e2f,
        10.0e4f,
        10.0e8f,
        10.0e16f,
        10.0e32f,
};

int fbp_cstr_to_f32(const char * src, float * value) {
    float x;
    float exp = 1.0f;
    const float * exp_pow_ptr = exp_pow;
    int32_t accum_int = 0;
    int32_t accum_fract = 0;
    int32_t accum_fact_pow = 0;
    bool is_neg = false;
    bool is_exp_neg = false;

    if ((NULL == src) || (NULL == value)) {
        return 1;
    }

    while (*src && _isspace((uint8_t) *src)) {
        ++src;
    }
    if (!*src) { // empty string.
        return 1;
    }

    // parse sign
    if (*src == '+') {
        ++src;
    } else if (*src == '-') {
        is_neg = true;
        ++src;
    }

    // parse integer component
    while (_isdigit(*src)) {
        accum_int = accum_int * 10 + (*src++ - '0');
    }
    x = (float) accum_int;

    // parse optional fractional component
    if (*src == '.') {
        ++src;
        while (_isdigit(*src)) {
            accum_fract = accum_fract * 10 + (*src++ - '0');
            accum_fact_pow *= 10;
        }
        x += accum_fract / (float) accum_fact_pow;
    }

    // parse optional exponent
    if ((*src == 'E') || (*src == 'e')) {
        ++src;
        accum_int = 0;
        // parse optional exponent sign
        if (*src == '+') {
            ++src;
        } else if (*src == '-') {
            is_exp_neg = true;
            ++src;
        }
        while (_isdigit(*src)) {
            accum_int = accum_int * 10 + (*src++ - '0');
        }
        if (accum_int > FLOAT_EXP_MAX) {
            accum_int = FLOAT_EXP_MAX;
        }
        // convert exponent into multiplier/divider using table lookup
        for (; accum_int; accum_int >>= 1, ++exp_pow_ptr) {
            if (accum_int & 1) {
                exp *= *exp_pow_ptr;
            }
        }
        x = is_exp_neg ? (x / exp) : (x * exp);
    }

    // parse optional floating point designator
    if ((*src == 'F') || (*src == 'f')) {
        ++src;
    }

    // ensure that we parsed the full string
    while (*src) {
        if (!_isspace((uint8_t) *src++)) { // did not parse full string
            return 1;
        }
    }

    *value = is_neg ? -x : x;
    return 0;
}
#endif

int fbp_u32_to_cstr(uint32_t u32, char * str, fbp_size_t str_size) {
    char buf[11];
    char *p = buf;
    if (!str || !str_size) {
        return FBP_ERROR_PARAMETER_INVALID;
    }
    *str = 0;
    while (u32) {
        uint32_t d = u32 / 10;
        uint32_t k = u32 - (d * 10);  // u32 % 10
        *p++ = '0' + k;
        u32 = d;
    }
    if (p == buf) {
        *p++ = '0';
    }
    if ((p - buf + 1) > str_size) {
        return FBP_ERROR_TOO_SMALL;
    }
    --p;
    while (p >= buf) {
        *str++ = *p--;
    }
    *str = 0;
    return 0;
}

int fbp_cstr_toupper(char * s) {
    if (NULL == s) {
        return 1;
    }
    while (*s) {
        *s = _toupper((uint8_t) *s);
        ++s;
    }
    return 0;
}

int fbp_cstr_to_index(char const * s, char const * const * table, int * index) {
    if ((NULL == s) || (NULL == table) || (NULL == index)) {
        return 2;
    }
    int idx = 0;
    while (*table) {
        if (strcmp(s, *table) == 0) {
            *index = idx;
            return 0;
        }
        ++idx;
        ++table;
    }
    return 1;
}

static const char * const true_table_[] = {"ON", "1", "TRUE", "YES", "ENABLE", "ENABLED", NULL};
static const char * const false_table[] = {"OFF", "0", "FALSE", "NO", "DISABLE", "DISABLED", "NULL", "NONE", NULL};

int fbp_cstr_to_bool(char const * s, bool * value) {
    char buffer[16]; // longer than any entry
    int index;
    if ((s == NULL) || (NULL == value)) {
        return 1;
    }
    fbp_cstr_array_copy(buffer, s);
    fbp_cstr_toupper(buffer);
    if (0 == fbp_cstr_to_index(buffer, true_table_, &index)) {
        *value = true;
        return 0;
    }
    if (0 == fbp_cstr_to_index(buffer, false_table, &index)) {
        *value = false;
        return 0;
    }
    return 1;
}

uint8_t fbp_cstr_hex_to_u4(char v) {
    if ((v >= '0') && (v <= '9')) {
        return v - '0';
    } else if ((v >= 'a') && (v <= 'f')) {
        return v - 'a' + 10;
    } else if ((v >= 'A') && (v <= 'F')) {
        return v - 'A' + 10;
    } else {
        return 0;
    }
}

char fbp_cstr_u4_to_hex(uint8_t v) {
    if (v < 10) {
        return ('0' + v);
    } else if (v < 16) {
        return ('A' + (v - 10));
    } else {
        return '0';
    }
}
