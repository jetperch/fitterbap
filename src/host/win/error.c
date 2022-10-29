/*
 * Copyright 2014-2022 Jetperch LLC
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

#include "fitterbap/host/win/error.h"

// https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
// This functions fills a caller-defined character buffer (pBuffer)
// of max length (cchBufferLength) with the human-readable error message
// for a Win32 error code (dwErrorCode).
//
// Returns TRUE if successful, or FALSE otherwise.
// If successful, pBuffer is guaranteed to be NUL-terminated.
// On failure, the contents of pBuffer are undefined.
BOOL GetErrorMessage(DWORD dwErrorCode, char * pBuffer, DWORD cchBufferLength) {
    char* p = pBuffer;
    if (cchBufferLength == 0) {
        return FALSE;
    }
    pBuffer[0] = 0;

    DWORD cchMsg = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                  NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
                                  dwErrorCode,
                                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                  pBuffer,
                                  cchBufferLength,
                                  NULL);

    while (*p) {
        if ((*p == '\n') || (*p == '\r')) {
            *p = 0;
            break;
        }
        ++p;
    }
    return (cchMsg > 0);
}
