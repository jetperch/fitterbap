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

#include "fitterbap/version.h"
#include "tinyprintf.h"


void fbp_version_u32_to_str(uint32_t u32, char * str, size_t size) {
    uint8_t major = FBP_VERSION_DECODE_U32_MAJOR(u32);
    uint8_t minor = FBP_VERSION_DECODE_U32_MINOR(u32);
    uint16_t patch = FBP_VERSION_DECODE_U32_PATCH(u32);
    tfp_snprintf(str, size, "%d.%d.%d", (int) major, (int) minor, (int) patch);
}
