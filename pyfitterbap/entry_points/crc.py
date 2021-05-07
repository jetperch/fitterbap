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

import numpy as np
from pyfitterbap import crc


def parser_config(p):
    """compute CRC."""
    p.add_argument('--data',
                   help='The CRC data.')
    return on_cmd


def on_cmd(args):
    if args.data is not None:
        x = np.array([int(x, 0) for x in args.data.split(',')], dtype=np.uint8)
        y = crc.crc32(0, x)
        print(f'0x{y:08x}')
        return 0
    return 1

