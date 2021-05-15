#!/usr/bin/env python3
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

"""
Update the project version.

Use the most recently CHANGELOG as the definitive version for:
- CHANGELOG.md
- CMakeLists.txt
- include/fitterbap/version.h
- pyfitterbap/version.py
"""

import os
import re

MYPATH = os.path.dirname(os.path.abspath(__file__))


def _str(version):
    return '.'.join([str(x) for x in version])


def _changelog_version():
    path = os.path.join(MYPATH, 'CHANGELOG.md')
    with open(path, 'rt') as f:
        for line in f:
            if line.startswith('## '):
                version = line.split(' ')[1]
                return [int(x) for x in version.split('.')]


def _cmakelists_update(version):
    regex = re.compile(r'(^\s*VERSION\s)')
    path = os.path.join(MYPATH, 'CMakeLists.txt')
    path_tmp = path + '.tmp'
    with open(path, 'rt') as rd:
        with open(path_tmp, 'wt') as wr:
            for line in rd:
                m = regex.match(line)
                if m is not None:
                    line = m[1] + _str(version) + '\n'
                wr.write(line)
    os.replace(path_tmp, path)


def _include_h_version(version):
    k = {'MAJOR': 0, 'MINOR': 1, 'PATCH': 2}
    regex = re.compile(r'#define FBP_VERSION_(MAJOR|MINOR|PATCH)\s')
    path = os.path.join(MYPATH, 'include', 'fitterbap', 'version.h')
    path_tmp = path + '.tmp'
    with open(path, 'rt') as rd:
        with open(path_tmp, 'wt') as wr:
            for line in rd:
                m = regex.match(line)
                if m is not None:
                    v = version[k[m[1]]]
                    line = f'#define FBP_VERSION_{m[1]} {v}\n'
                wr.write(line)
    os.replace(path_tmp, path)


def _py_version(version):
    path = os.path.join(MYPATH, 'pyfitterbap', 'version.py')
    path_tmp = path + '.tmp'
    with open(path, 'rt') as rd:
        with open(path_tmp, 'wt') as wr:
            for line in rd:
                if line.startswith('__version__'):
                    line = f'__version__ = "{_str(version)}"\n'
                wr.write(line)
    os.replace(path_tmp, path)


def run():
    version = _changelog_version()
    print(f'Version = {_str(version)}')
    _cmakelists_update(version)
    _include_h_version(version)
    _py_version(version)
    return 0


if __name__ == '__main__':
    run()
