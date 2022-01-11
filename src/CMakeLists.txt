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

set(SOURCES
        collections/list.c
        cli.c
        crc.c
        cstr.c
        ec.c
        event_manager.c
        fsm.c
        lfsr.c
        log.c
        logh.c
        pattern_32a.c
        pubsub.c
        time.c
        topic.c
        topic_list.c
        union.c
        collections/ring_buffer_msg.c
        comm/data_link.c
        comm/framer.c
        comm/log_port.c
        comm/port.c
        comm/port0.c
        comm/pubsub_port.c
        comm/stack.c
        comm/timesync.c
        comm/transport.c
        memory/block.c
        memory/buffer.c
        memory/object_pool.c
        memory/pool.c
        ${PLATFORM_SRC}
)

# Respect if TINYPRINTF has already been included, such as with RTOS build
get_property(tinyprintf_property GLOBAL PROPERTY TINYPRINTF)
if (NOT tinyprintf_property)
    message(STATUS "Fitterbap add tinyprintf")
    set(SOURCES ${SOURCES} ../third-party/tinyprintf/tinyprintf.c)
endif()
set_property(GLOBAL PROPERTY TINYPRINTF 1)

if ("${FITTERBAP_OS}" STREQUAL "FREERTOS")
    set(SOURCES_HOST
            host/freertos/os_mutex.c
            host/freertos/os_task.c)

elseif ("${FITTERBAP_OS}" STREQUAL "WIN32")
    set(SOURCES_HOST
            host/win/uart.c
            host/win/platform.c
            host/win/comm.c
            host/platform_alloc.c)
else()
    set(SOURCES_HOST "")
endif()

message(STATUS "SOURCES_HOST = ${SOURCES_HOST}")

set(SOURCES ${SOURCES} ${SOURCES_HOST})

foreach(f IN LISTS SOURCES)
    SET_FILENAME("${f}")
endforeach()

add_library(fitterbap_objlib OBJECT ${SOURCES})
