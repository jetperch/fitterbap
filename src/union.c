/*
 * Copyright 2014-2021 Jetperch LLC
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

#include "fitterbap/union.h"
#include "fitterbap/cstr.h"
#include "fitterbap/ec.h"
#include <string.h>


bool fbp_union_eq(const struct fbp_union_s * v1, const struct fbp_union_s * v2) {
    if (v1->type != v2->type) {
        return false;
    }
    switch (v1->type) {
        case FBP_UNION_NULL: return true;
        case FBP_UNION_JSON: /* intentional fall-through */
        case FBP_UNION_STR: {
            size_t sz1 = v1->size ? v1->size : strlen(v1->value.str);
            size_t sz2 = v2->size ? v2->size : strlen(v2->value.str);
            if (sz1 != sz2) {
                return false;
            }
            return 0 == memcmp(v1->value.str, v2->value.str, sz1);
        }
        case FBP_UNION_BIN:
            if (v1->size != v2->size) {
                return false;
            }
            return 0 == memcmp(v1->value.bin, v2->value.bin, v1->size);
#if FBP_CONFIG_USE_FLOAT32
        case FBP_UNION_F32: return v1->value.f32 == v2->value.f32;
#endif
#if FBP_CONFIG_USE_FLOAT64
        case FBP_UNION_F64: return v1->value.f64 == v2->value.f64;
#endif
        case FBP_UNION_U8: return v1->value.u8 == v2->value.u8;
        case FBP_UNION_U16: return v1->value.u16 == v2->value.u16;
        case FBP_UNION_U32: return v1->value.u32 == v2->value.u32;
        case FBP_UNION_U64: return v1->value.u64 == v2->value.u64;
        case FBP_UNION_I8: return v1->value.i8 == v2->value.i8;
        case FBP_UNION_I16: return v1->value.i16 == v2->value.i16;
        case FBP_UNION_I32: return v1->value.i32 == v2->value.i32;
        case FBP_UNION_I64: return v1->value.i64 == v2->value.i64;
        default:
            return false;
    }
}

bool fbp_union_eq_exact(const struct fbp_union_s * v1, const struct fbp_union_s * v2) {
    if ((v1->flags != v2->flags) || (v1->op != v2->op) || (v1->app != v2->app) || (v1->size != v2->size)) {
        return false;
    }
    return fbp_union_eq(v1, v2);
}

void fbp_union_widen(struct fbp_union_s * v) {
    switch (v->type) {
#if FBP_CONFIG_USE_FLOAT32
        case FBP_UNION_F32:
#if FBP_CONFIG_USE_FLOAT64
            v->type = FBP_UNION_F64;
            v->value.f64 = (double) v->value.f32;
#endif
            break;
#endif
        case FBP_UNION_U8:  v->type = FBP_UNION_U64; v->value.u64 = v->value.u8; break;
        case FBP_UNION_U16: v->type = FBP_UNION_U64; v->value.u64 = v->value.u16; break;
        case FBP_UNION_U32: v->type = FBP_UNION_U64; v->value.u64 = v->value.u32; break;
        case FBP_UNION_U64: break;
        case FBP_UNION_I8:  v->type = FBP_UNION_I64; v->value.i64 = v->value.i8; break;
        case FBP_UNION_I16: v->type = FBP_UNION_I64; v->value.i64 = v->value.i16; break;
        case FBP_UNION_I32: v->type = FBP_UNION_I64; v->value.i64 = v->value.i32; break;
        case FBP_UNION_I64: break;
        default: break;
    }
}

bool fbp_union_equiv(const struct fbp_union_s * v1, const struct fbp_union_s * v2) {
    struct fbp_union_s x = *v1;
    struct fbp_union_s y = *v2;
    fbp_union_widen(&x);
    fbp_union_widen(&y);
    if (fbp_union_eq(&x, &y)) {
        return true;
    }

    switch (x.type) {
        case FBP_UNION_U64:
            switch (y.type) {
                case FBP_UNION_U64: return x.value.u64 == y.value.u64;
                case FBP_UNION_I64: return (x.value.i64 == y.value.i64) && (x.value.i64 >= 0) && (y.value.i64 <= INT64_MAX);
                default: return false;
            }
        case FBP_UNION_I64:
            switch (y.type) {
                case FBP_UNION_U64: return (x.value.i64 == y.value.i64) && (x.value.i64 >= 0) && (y.value.i64 <= INT64_MAX);
                case FBP_UNION_I64: return x.value.i64 == y.value.i64;
                default: return false;
            }
        default:
            return false;
    }
}

#define AS_TYPE(condition) rv = (condition) ? FBP_ERROR_PARAMETER_INVALID : 0; break


int32_t fbp_union_as_type(struct fbp_union_s * x, uint8_t type) {
    int32_t rv = 0;
    fbp_union_widen(x);
    if (x->type == type) {
        return rv;
    }
    switch (x->type) {
        case FBP_UNION_U64:
            switch (type) {
                case FBP_UNION_U8:  AS_TYPE(x->value.u64 > UINT8_MAX);
                case FBP_UNION_U16: AS_TYPE(x->value.u64 > UINT16_MAX);
                case FBP_UNION_U32: AS_TYPE(x->value.u64 > UINT32_MAX);
                case FBP_UNION_U64: break;
                case FBP_UNION_I8:  AS_TYPE(x->value.u64 > INT8_MAX);
                case FBP_UNION_I16: AS_TYPE(x->value.u64 > INT16_MAX);
                case FBP_UNION_I32: AS_TYPE(x->value.u64 > INT32_MAX);
                case FBP_UNION_I64: break;
                default: rv = FBP_ERROR_PARAMETER_INVALID; break;
            }
            if (!rv) {
                x->type = type;
            }
            break;
        case FBP_UNION_I64: {
            int64_t v = x->value.i64;
            switch (type) {
                case FBP_UNION_U8:  AS_TYPE((v < 0) || (v > UINT8_MAX));
                case FBP_UNION_U16: AS_TYPE((v < 0) || (v > UINT16_MAX));
                case FBP_UNION_U32: AS_TYPE((v < 0) || (v > UINT32_MAX));
                case FBP_UNION_U64: break;
                case FBP_UNION_I8:  AS_TYPE((v < 0) || (v > INT8_MAX));
                case FBP_UNION_I16: AS_TYPE((v < 0) || (v > INT16_MAX));
                case FBP_UNION_I32: AS_TYPE((v < 0) || (v > INT32_MAX));
                case FBP_UNION_I64: break;
                default: rv = FBP_ERROR_PARAMETER_INVALID; break;
            }
            if (!rv) {
                x->type = type;
            }
            break;
        }
        default: rv = FBP_ERROR_PARAMETER_INVALID; break;
    }
    return rv;
}


int32_t fbp_union_to_bool(const struct fbp_union_s * value, bool * rv) {
    switch (value->type) {
        case FBP_UNION_NULL: *rv = false; return 0;
        case FBP_UNION_STR:  return fbp_cstr_to_bool(value->value.str, rv) ? FBP_ERROR_PARAMETER_INVALID : 0;
        case FBP_UNION_JSON: return fbp_cstr_to_bool(value->value.str, rv) ? FBP_ERROR_PARAMETER_INVALID : 0;
        case FBP_UNION_BIN: *rv = false; return FBP_ERROR_PARAMETER_INVALID;
#if FBP_CONFIG_USE_FLOAT32
        case FBP_UNION_F32: *rv = value->value.f32 != 0.0f; return 0;
#endif
#if FBP_CONFIG_USE_FLOAT64
        case FBP_UNION_F64: *rv = value->value.f64 != 0.0; return 0;
#endif
        case FBP_UNION_U8:  *rv = value->value.u8 != 0;  return 0;
        case FBP_UNION_U16: *rv = value->value.u16 != 0; return 0;
        case FBP_UNION_U32: *rv = value->value.u32 != 0; return 0;
        case FBP_UNION_U64: *rv = value->value.u64 != 0; return 0;
        case FBP_UNION_I8:  *rv = value->value.i8 != 0;  return 0;
        case FBP_UNION_I16: *rv = value->value.i16 != 0; return 0;
        case FBP_UNION_I32: *rv = value->value.i32 != 0; return 0;
        case FBP_UNION_I64: *rv = value->value.i64 != 0; return 0;
        default: *rv = false; return FBP_ERROR_PARAMETER_INVALID;
    }
}
