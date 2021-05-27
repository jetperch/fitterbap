# Copyright 2021 Jetperch LLC
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


import numpy as np


def run():
    counts = np.zeros(256, dtype=np.uint8)
    for i in range(256):
        k = i
        while k:
            if k & 1:
                counts[i] += 1
            k = k >> 1
    counts_str = [str(x) for x in counts]
    for i in range(0, 256, 16):
        print(', '.join(counts_str[i:i+16]) + ',')


if __name__ == '__main__':
    run()
