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
 * @brief Cyclic Redundancy Codes (CRC)
 */

#ifndef FBP_CRC_H__
#define FBP_CRC_H__

#include "fitterbap/cmacro_inc.h"
#include <stdint.h>

/**
 * @ingroup fbp_core
 * @defgroup fbp_crc CRC
 *
 * @brief Cyclic Redundancy Codes (CRC).
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief Compute the CRC-CCITT-8
 *
 * @param crc The existing value for the crc which is used for continued block
 *      computations.  Pass 0 for the first block.
 * @param data The data for the CRC computation.
 * @param length The number of total_bytes in data.
 * @return The computed CRC-8.
 *
 * This function uses the 0x83 polynomial
 */
FBP_API uint8_t fbp_crc_ccitt_8(uint8_t crc, uint8_t const *data, uint32_t length);

/**
 * @brief Compute the CRC-CCITT-16 in one's compliment form.
 *
 * @param crc The existing value for the crc which is used for continued block
 *      computations.  Pass 0 for the first block.
 * @param data The data for the CRC computation.
 * @param length The number of total_bytes in data.
 * @return The computed CRC-16.
 *
 * Although this implementation uses the CCITT 0x1021 polynomial, the
 * output is XOR'ed with 0xffff so that calls to this function may be
 * chained.
 *
 * @see http://srecord.sourceforge.net/crc16-ccitt.html
 * @see https://www.lammertbies.nl/comm/info/crc-calculation.html
 */
FBP_API uint16_t fbp_crc_ccitt_16(uint16_t crc, uint8_t const *data, uint32_t length);

/**
 * @brief Compute the CRC-32
 *
 * @param crc The existing value for the crc which is used for continued block
 *      computations.  Pass 0 for the first block.
 * @param data The data for the CRC computation.
 * @param length The number of total_bytes in data.
 * @return The computed CRC-32.
 *
 * @see http://create.stephan-brumme.com/crc32/
 * @see https://pycrc.org
 */
FBP_API uint32_t fbp_crc32(uint32_t crc, uint8_t const *data, uint32_t length);

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_CRC_H__ */
