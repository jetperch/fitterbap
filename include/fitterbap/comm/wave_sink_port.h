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
 * @brief Waveform sink port.
 */

#ifndef FBP_COMM_WAVEFORM_SINK_PORT_H_
#define FBP_COMM_WAVEFORM_SINK_PORT_H_

#include "fitterbap/comm/port.h"
#include "fitterbap/comm/wave_port.h"

/**
 * @ingroup fbp_comm
 * @defgroup fbp_comm_waveform_sink_port Waveform Sink Port
 *
 * @brief Waveform sink port.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief Allocate an waveform port instance.
 *
 * @return The new instance or NULL on error.
 */
FBP_API struct fbp_port_api_s * fbp_wave_sink_factory();

FBP_CPP_GUARD_END

/** @} */

#endif  /* FBP_COMM_WAVEFORM_SINK_PORT_H_ */
