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

/**
 * @file
 *
 * @brief FBP library version.
 */

#ifndef FBP_VERSION_H_
#define FBP_VERSION_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @ingroup fbp
 * @defgroup fbp_version Version
 *
 * @brief Fitterbap Version.
 *
 * @{
 */

// Use version_update.py to update.
#define FBP_VERSION_MAJOR 0
#define FBP_VERSION_MINOR 5
#define FBP_VERSION_PATCH 1

/**
 * \brief The maximum version string length.
 *
 * The actual length is 14 bytes (MMM.mmm.ppppp\x0), but round up
 * to simplify packing.
 */
#define FBP_VERSION_STR_LENGTH_MAX  (16)

/**
 * \brief Macro to encode version to uint32_t
 *
 * \param major The major release number (0 to 255)
 * \param minor The minor release number (0 to 255)
 * \param patch The patch release number (0 to 65535)
 * \returns The 32-bit encoded version number.
 */
#define FBP_VERSION_ENCODE_U32(major, minor, patch) \
    ( (( ((uint32_t) (major)) &   0xff) << 24) | \
      (( ((uint32_t) (minor)) &   0xff) << 16) | \
      (( ((uint32_t) (patch)) & 0xffff) <<  0) )

/// Decode the major version from a U32 encoded version.
#define FBP_VERSION_DECODE_U32_MAJOR(ver_u32_)   ((uint8_t) ((ver_u32_ >> 24) & 0xff))
/// Decode the minor version from a U32 encoded version.
#define FBP_VERSION_DECODE_U32_MINOR(ver_u32_)   ((uint8_t) ((ver_u32_ >> 16) & 0xff))
/// Decode the patch version from a U32 encoded version.
#define FBP_VERSION_DECODE_U32_PATCH(ver_u32_)   ((uint16_t) ((ver_u32_ >> 0) & 0xffff))

/**
 * \brief Internal macro to convert argument to string.
 *
 * \param x The argument to convert to a string.
 * \return The string version of x.
 */
#define FBP_VERSION__STR(x) #x

/**
 * \brief Macro to create the version string separated by "." characters.
 *
 * \param major The major release number (0 to 255)
 * \param minor The minor release number (0 to 255)
 * \param patch The patch release number (0 to 65535)
 * \returns The firmware string.
 */
#define FBP_VERSION_ENCODE_STR(major, minor, patch) \
        FBP_VERSION__STR(major) "." FBP_VERSION__STR(minor) "." FBP_VERSION__STR(patch)

/// The FBP version as uint32_t
#define FBP_VERSION_U32 FBP_VERSION_ENCODE_U32(FBP_VERSION_MAJOR, FBP_VERSION_MINOR, FBP_VERSION_PATCH)

/// The FBP version as "major.minor.patch" string
#define FBP_VERSION_STR FBP_VERSION_ENCODE_STR(FBP_VERSION_MAJOR, FBP_VERSION_MINOR, FBP_VERSION_PATCH)

/**
 * \brief Convert a u32 encoded version as a string.
 *
 * \param u32[in] The u32 encoded version.
 * \param str[out] The output string, which should have at least 14
 *      bytes available to avoid truncation.
 * \param size[in] The number of bytes available in str.
 */
void fbp_version_u32_to_str(uint32_t u32, char * str, size_t size);

/** @} */

#endif /* FBP_VERSION_H_ */
