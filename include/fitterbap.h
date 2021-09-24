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
 * @brief Embedded systems library for C (FBP)
 *
 * The top-level FBP header file automatically includes select core components
 * of the FBP library.
 */

#ifndef FBP_H__
#define FBP_H__

#include "fitterbap/common_header.h"
#include "fitterbap/argchk.h"
#include "fitterbap/assert.h"
#include "fitterbap/cdef.h"
#include "fitterbap/dbc.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include "fitterbap/time.h"
#include "fitterbap/version.h"

/**
 * @defgroup fbp Fitterbap
 *
 * @brief Firmware toolkit to enable reliable best architecture practices (FBP).
 */

/**
 * @defgroup fbp_core Core
 * @ingroup fbp
 *
 * @brief FBP Core.
 */

/**
 * @defgroup fbp_collections Collections
 * @ingroup fbp
 *
 * @brief FBP Collections.
 */

/**
 * @defgroup fbp_comm Comm
 * @ingroup fbp
 *
 * @brief FBP Comm.
 */

/**
 * @defgroup fbp_host Host
 * @ingroup fbp
 *
 * @brief FBP Host.
 */

/**
 * @defgroup fbp_memory Memory
 * @ingroup fbp
 *
 * @brief FBP Memory.
 */

/**
 * @defgroup fbp_os OS
 * @ingroup fbp
 *
 * @brief FBP Operating System.
 */

/**
 * @defgroup fbp_platform Platform
 * @ingroup fbp
 *
 * @brief FBP Platform.
 */

#endif /* FBP_H__ */
