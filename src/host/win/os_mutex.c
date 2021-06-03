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


#include "fitterbap/os/mutex.h"
#include "fitterbap/assert.h"
#include "fitterbap/ec.h"
#include "fitterbap/log.h"
#include <windows.h>


#define MUTEX_LOCK_TIMEOUT_MS  (500)

fbp_os_mutex_t fbp_os_mutex_alloc() {
    fbp_os_mutex_t mutex = CreateMutex(
            NULL,                   // default security attributes
            FALSE,                  // initially not owned
            NULL);                  // unnamed mutex
    FBP_ASSERT_ALLOC(mutex);
    return mutex;
}

void fbp_os_mutex_free(fbp_os_mutex_t mutex) {
    if (mutex) {
        CloseHandle(mutex);
    }
}

void fbp_os_mutex_lock(fbp_os_mutex_t mutex) {
    if (mutex) {
        DWORD rc = WaitForSingleObject(mutex, MUTEX_LOCK_TIMEOUT_MS);
        if (WAIT_OBJECT_0 != rc) {
            FBP_LOG_CRITICAL("mutex lock failed: %d", rc);
            FBP_FATAL("mutex lock failed");
        }
    } else {
        FBP_LOGD1("lock, but mutex is null");
    }
}

void fbp_os_mutex_unlock(fbp_os_mutex_t mutex) {
    if (mutex) {
        if (!ReleaseMutex(mutex)) {
            FBP_LOG_CRITICAL("mutex unlock failed");
            FBP_FATAL("mutex unlock failed");
        }
    } else {
        FBP_LOGD1("unlock, but mutex is null");
    }
}
