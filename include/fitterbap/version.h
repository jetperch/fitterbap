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
#define FBP_VERSION_MINOR 3
#define FBP_VERSION_PATCH 1

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

/** @} */

#endif /* FBP_VERSION_H_ */
