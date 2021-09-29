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

cmake_minimum_required (VERSION 3.1)
set(PARENT_PROJECT_DIR ${PROJECT_SOURCE_DIR})
project(FITTERBAP
        VERSION 0.5.0
        LANGUAGES C)
SET(PROJECT_PREFIX FBP)
SET(VERSION_STRING "${PROJECT_VERSION}")
SET(VERSION_UNDERSCORE "${PROJECT_VERSION_MAJOR}_${PROJECT_VERSION_MINOR}_${PROJECT_VERSION_PATCH}")

if (NOT PARENT_PROJECT_DIR)
    set(FITTERBAP_TOPLEVEL 1)
else()
    set(FITTERBAP_TOPLEVEL 0)
endif()

if(DEFINED FITTERBAP_OS)
    # use the provided value
elseif (WIN32)
    SET(FITTERBAP_OS "WIN32" CACHE STRING "Target operating system")
elseif (LINUX)
    SET(FITTERBAP_OS "LINUX" CACHE STRING "Target operating system")
else ()
    message(FATAL_ERROR "Could not detect target OS")
endif()
message(STATUS "Fitterbap OS = ${FITTERBAP_OS}")

option(FITTERBAP_DOCS "Use Doxygen to create the HTML based Host API documentation" OFF)
option(FITTERBAP_UNIT_TEST "Build the fitterbap unit tests" ON)
option(FITTERBAP_EXAMPLES "Build the fitterbap examples" ON)

function (SET_FILENAME _filename)
    get_filename_component(b ${_filename} NAME)
    set_source_files_properties(${_filename} PROPERTIES
        COMPILE_DEFINITIONS "__FILENAME__=\"${b}\"")
endfunction (SET_FILENAME)


if (FITTERBAP_TOPLEVEL AND CMAKE_COMPILER_IS_GNUCC)
    add_definitions(-Wall -Wextra -Wpedantic -Werror -fPIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()
remove_definitions(-D__cplusplus)
if (WIN32 AND CMAKE_COMPILER_IS_GNUCC)
    # Ugh, mingw
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format")
endif()


add_subdirectory(third-party)

set(FITTERBAP_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
    CACHE INTERNAL "fitterbap source path" FORCE
)
set(FITTERBAP_INCLUDE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${THIRD_PARTY_INCLUDES}
        CACHE INTERNAL "fitterbap include paths" FORCE
        )

set(FITTERBAP_LIBS fitterbap ${THIRD_PARTY_LIBS} CACHE INTERNAL "fitterbap libraries" FORCE)
set(FITTERBAP_DEPENDS fitterbap CACHE INTERNAL "fitterbap dependencies" FORCE)

include_directories(${FITTERBAP_INCLUDE})
if (${FITTERBAP_TOPLEVEL})
    message(STATUS "Fitterbap: IS toplevel")
    include_directories(${CMAKE_SOURCE_DIR}/port/include)
else()
    message(STATUS "Fitterbap: is NOT toplevel")
endif()
add_subdirectory(src)

if(FITTERBAP_UNIT_TEST AND FITTERBAP_TOPLEVEL AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(test)
endif()

if(FITTERBAP_DOCS AND FITTERBAP_TOPLEVEL)
    FIND_PACKAGE(Doxygen)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR
                "Doxygen is needed to build the documentation. Please install it")
    endif()
    #-- Configure the Template Doxyfile for our specific project
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile  @ONLY IMMEDIATE)
    #-- Add a custom target to run Doxygen when ever the project is built
    add_custom_target(docs ALL
                      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      SOURCES ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      DEPENDS fitterbap)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/ DESTINATION doc)
endif()
