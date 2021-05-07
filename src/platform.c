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

#include "fitterbap/platform.h"
#include "fitterbap/assert.h"


static void * alloc_default(fbp_size_t size_bytes) {
    (void) size_bytes;
    FBP_FATAL("no alloc");
    return 0;
}

static void free_default(void * ptr) {
    (void) ptr;
    FBP_FATAL("no free");
}

static fbp_alloc_fn alloc_ = alloc_default;
static fbp_free_fn free_ = free_default;

void fbp_allocator_set(fbp_alloc_fn alloc, fbp_free_fn free) {
    alloc_ = (0 != alloc) ? alloc : alloc_default;
    free_ = (0 != free) ? free : free_default;;
}

void * fbp_alloc(fbp_size_t size_bytes) {
    void * ptr = alloc_(size_bytes);
    FBP_ASSERT_ALLOC(ptr);
    return ptr;
}

void fbp_free(void * ptr) {
    FBP_ASSERT(0 != ptr);
    free_(ptr);
}

