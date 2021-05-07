/*
 * Copyright 2021 Jetperch LLC
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
 * @brief Waveform port definitions.
 */

#ifndef FBP_COMM_WAVEFORM_PORT_H_
#define FBP_COMM_WAVEFORM_PORT_H_

#include "fitterbap/comm/port.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_waveform_port Waveform Port Def
 *
 * @brief Waveform port definitions.
 *
 * @{
 */

FBP_CPP_GUARD_START

/*
 * FEATURES to implement
 *
 * - Send & receive waveform data for float32
 * - Send & receive waveform data for signed & unsigned integers of various widths
 * - Send & receive timing data: sample_id & UTC pairs
 * - Compression
 *
 * port_data[15:12] == 0: unsupported
 * port_data[15:12] == 1: start
 *     - 64-bit starting sample_id, ignoring div
 *     - 32-bit data type
 * port_data[15:12] == 2: stop
 *     - 64-bit ending sample_id, ignoring div
 * port_data[15:12] == 3: skip (explicit indicator)
 *     - 64-bit sample_id of first missing sample
 *     - 64-bit sample_id of last missing sample
 * port_data[15] == 1: data packet
 *     - port_data[14:8], reserved, set to 0
 *     - port_data[7:0], compression type, 0=no compression
 *     - 32-bit sample_id[31:0]
 *     - waveform data, packed
 */

#define FBP_WAVEP_DTYPE_F32 (0x0033)

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_WAVEFORM_PORT_H_ */
