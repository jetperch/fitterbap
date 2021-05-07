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

#include "fitterbap/ec.h"
#include "fitterbap/argchk.h"
#include "fitterbap/cdef.h"


FBP_STATIC_ASSERT(FBP_ARGCHK_FAIL_RETURN_CODE_DEFAULT == FBP_ERROR_PARAMETER_INVALID,
                   argchk_return_code_mismatch);

#define SWITCH_NAME(NAME, TEXT) case FBP_ERROR_ ## NAME: return #NAME;
#define SWITCH_DESCRIPTION(NAME, TEXT) case FBP_ERROR_ ## NAME: return TEXT;


const char * fbp_error_code_name(int ec) {
    switch (ec) {
        FBP_ERROR_CODES(SWITCH_NAME);
        default: return "UNKNOWN";
    }
}

const char * fbp_error_code_description(int ec) {
    switch (ec) {
        FBP_ERROR_CODES(SWITCH_DESCRIPTION);
        default: return "Unknown error";
    }
}
