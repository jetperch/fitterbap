# Copyright 2020-2021 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


from .c_crc cimport *
from libc.stdint cimport uint8_t, uint16_t, uint32_t
cimport numpy as np
import numpy as np


def crc_ccitt_8(crc, data):
    cdef np.uint8_t [::1] d_c
    data = np.ascontiguousarray(data, dtype=np.uint8)
    d_c = data
    return fbp_crc_ccitt_8(<uint8_t> crc, &d_c[0], len(data))


def crc_ccitt_16(crc, data):
    cdef np.uint8_t [::1] d_c
    data = np.ascontiguousarray(data, dtype=np.uint8)
    d_c = data
    return fbp_crc_ccitt_16(<uint16_t> crc, &d_c[0], len(data))

def crc32(crc, data):
    cdef np.uint8_t [::1] d_c
    data = np.ascontiguousarray(data, dtype=np.uint8)
    d_c = data
    return fbp_crc32(<uint32_t> crc, &d_c[0], len(data))
