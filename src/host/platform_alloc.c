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

#include "fitterbap/common_header.h"
#include <windows.h>

void * fbp_alloc_(fbp_size_t size_bytes) {
    void * ptr = malloc(size_bytes);
    FBP_ASSERT_ALLOC(ptr);
    return ptr;
}

void fbp_free_(void * ptr) {
    free(ptr);
}
