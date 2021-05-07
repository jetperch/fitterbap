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

if ((${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") AND ${CMAKE_CROSSCOMPILING})
    SET(FITTERBAP_PLATFORM "ARM")
else()
    SET(FITTERBAP_PLATFORM "STDLIB")
endif()

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
        pattern_32a.c
        platform.c
        pubsub.c
        union.c
        collections/ring_buffer_msg.c
        comm/data_link.c
        comm/framer.c
        comm/port0.c
        comm/pubsub_port.c
        comm/stack.c
        comm/transport.c
        memory/block.c
        memory/buffer.c
        memory/object_pool.c
        memory/pool.c
        ${PLATFORM_SRC}
)


if ("${PLATFORM}" STREQUAL "FREERTOS")
    set(SOURCES_HOST
            host/freertos/os_mutex.c
            host/freertos/os_task.c)

elseif (WIN32)
    set(SOURCES_HOST
            host/win/os_mutex.c
            host/win/os_task.c
            host/win/platform.c
            host/win/uart.c
            host/win/uart_thread.c
            host/win/comm.c)
else()
    set(SOURCES_HOST "")
endif()

message(STATUS "SOURCES_HOST = ${SOURCES_HOST}")

set(SOURCES ${SOURCES} ${SOURCES_HOST})

foreach(f IN LISTS SOURCES)
    SET_FILENAME("${f}")
endforeach()

add_library(fitterbap_objlib OBJECT ${SOURCES})

if(${FITTERBAP_TOPLEVEL})
    add_library(fitterbap SHARED $<TARGET_OBJECTS:fitterbap_objlib> lib.c)
else()
    add_library(fitterbap STATIC $<TARGET_OBJECTS:fitterbap_objlib>)
    add_library(fitterbap_lib OBJECT lib.c)
endif()
add_dependencies(fitterbap fitterbap_objlib)
