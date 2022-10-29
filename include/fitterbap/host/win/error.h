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

/**
 * @file
 *
 * @brief Windows error handling support.
 */

#ifndef FBP_HOST_WIN_ERROR_H__
#define FBP_HOST_WIN_ERROR_H__

#include <windows.h>
#include "fitterbap/cmacro_inc.h"

FBP_CPP_GUARD_START

BOOL GetErrorMessage(DWORD dwErrorCode, char * pBuffer, DWORD cchBufferLength);

#define WINDOWS_LOGE(format, ...) { \
    char error_msg_[64]; \
    DWORD error_ = GetLastError(); \
    GetErrorMessage(error_, error_msg_, sizeof(error_msg_)); \
    FBP_LOGE(format ": %d: %s", __VA_ARGS__, (int) error_, error_msg_); \
}

FBP_CPP_GUARD_END

#endif  /* FBP_HOST_WIN_ERROR_H__ */

