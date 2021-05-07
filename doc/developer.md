<!--
# Copyright 2014-2021 Jetperch LLC
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
-->

# Developer

## Code cleanup

To remove trailing blanks:: 

    find . -iname "*.c" -o -iname "*.h" -o -iname "*.txt" -type f -exec sed -i 's/[[:space:]]*$//' {} \;

## References

Operating systems

* [POSIX](http://pubs.opengroup.org/onlinepubs/9699919799/)
* [NuttX](http://www.nuttx.org/)
* [polymcu](https://github.com/labapart/polymcu) : CMSIS OS wrappers
* [FreeRTOS](http://www.freertos.org/)

C standard library implementations

* [newlib](https://sourceware.org/newlib/)
* [PDClib](http://pdclib.e43.eu/)
* [uclibc](http://www.uclibc.org/)

Using clib:

* printf to [ITM](http://blog.atollic.com/cortex-m-debugging-printf-redirection-to-a-debugger-console-using-swv/itm-part-1)
* printf to [uart](http://www.openstm32.org/forumthread1055)


Linux

* [EmbToolkit](https://www.embtoolkit.org/)
* BuildRoot

Switch debouncing

* http://www.ganssle.com/debouncing.htm
