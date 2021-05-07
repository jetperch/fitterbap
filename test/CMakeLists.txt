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

#add_definitions(-Wno-format)

set(TEST_FILES  hal.c)
add_library(test_objlib OBJECT ${TEST_FILES})

set(objects
        $<TARGET_OBJECTS:fitterbap_objlib>
        $<TARGET_OBJECTS:test_objlib>)

set(dependencies
        fitterbap_objlib
        test_objlib
        cmocka)
include_directories(${CMOCKA_INCLUDE})
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

function (ADD_CMOCKA_TEST _testName)
    set(TARGET ${_testName})
    SET_FILENAME("${_testName}.c")
    add_executable(${_testName} "${_testName}.c" ${objects})
    add_dependencies(${TARGET} fitterbap_objlib test_objlib cmocka)
    target_link_libraries(${_testName} cmocka)
    add_test(${_testName} ${CMAKE_CURRENT_BINARY_DIR}/${_testName})
endfunction (ADD_CMOCKA_TEST)

add_subdirectory(collections)
add_subdirectory(memory)
add_subdirectory(comm)
ADD_CMOCKA_TEST(argchk_test)
ADD_CMOCKA_TEST(cdef_test)
ADD_CMOCKA_TEST(cli_test)
ADD_CMOCKA_TEST(crc_test)
ADD_CMOCKA_TEST(cstr_test)
ADD_CMOCKA_TEST(dbc_test)
ADD_CMOCKA_TEST(ec_test)
ADD_CMOCKA_TEST(event_manager_test)
ADD_CMOCKA_TEST(fsm_test)
ADD_CMOCKA_TEST(lfsr_test)
ADD_CMOCKA_TEST(log_test)
ADD_CMOCKA_TEST(pattern_32a_test)
ADD_CMOCKA_TEST(platform_test)
ADD_CMOCKA_TEST(pubsub_test)
ADD_CMOCKA_TEST(time_test)
ADD_CMOCKA_TEST(union_test)

# test static assert compile failures
# http://stackoverflow.com/questions/30155619/expected-build-failure-tests-in-cmake
add_executable(cdef_static_assert_test cdef_static_assert_test.c)
set_target_properties(cdef_static_assert_test PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE)
add_test(NAME cdef_static_assert_test
        COMMAND ${CMAKE_COMMAND} --build . --target cdef_static_assert_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(cdef_static_assert_test PROPERTIES WILL_FAIL TRUE)
