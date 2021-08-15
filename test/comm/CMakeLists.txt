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

ADD_CMOCKA_TEST(data_link_test)
ADD_CMOCKA_TEST(framer_test)

# transport_test special build to break data_link dependency
SET_FILENAME("transport_test.c")
add_executable(transport_test transport_test.c ../../src/comm/transport.c ../../src/platform.c ../../src/log.c $<TARGET_OBJECTS:test_objlib>)
add_dependencies(transport_test test_objlib cmocka)
target_link_libraries(transport_test cmocka)
add_test(transport_test ${CMAKE_CURRENT_BINARY_DIR}/transport_test)

# port0_test special build to break dependencies
SET_FILENAME("port0_test.c")
add_executable(port0_test port0_test.c
        ../../src/cstr.c
        ../../src/collections/ring_buffer_msg.c
        ../../src/comm/port0.c
        ../../src/fsm.c
        ../../src/pubsub.c
        ../../src/platform.c
        ../../src/union.c
        ../../src/topic_list.c
        ../../src/log.c
        $<TARGET_OBJECTS:test_objlib>)
add_dependencies(port0_test test_objlib cmocka)
target_link_libraries(port0_test cmocka)
add_test(port0_test ${CMAKE_CURRENT_BINARY_DIR}/port0_test)


# timesync_test special build to break dependencies
SET_FILENAME("timesync_test.c")
add_executable(timesync_test timesync_test.c
        ../../src/comm/timesync.c
        ../../src/platform.c
        ../../src/log.c
        $<TARGET_OBJECTS:test_objlib>)
add_dependencies(timesync_test test_objlib cmocka)
target_link_libraries(timesync_test cmocka)
add_test(timesync_test ${CMAKE_CURRENT_BINARY_DIR}/timesync_test)


# pubsub_port_test special build to break dependencies
SET_FILENAME("pubsub_port_test.c")
add_executable(pubsub_port_test
        pubsub_port_test.c
        ../../src/comm/pubsub_port.c
        ../../src/cstr.c
        ../../src/event_manager.c
        ../../src/fsm.c
        ../../src/platform.c
        ../../src/log.c
        ../../src/topic_list.c
        $<TARGET_OBJECTS:test_objlib>)
add_dependencies(pubsub_port_test test_objlib cmocka)
target_link_libraries(pubsub_port_test cmocka)
add_test(pubsub_port_test ${CMAKE_CURRENT_BINARY_DIR}/pubsub_port_test)

# log_port_test special build to break dependencies
SET_FILENAME("log_port_test.c")
add_executable(log_port_test log_port_test.c
        ../../src/comm/log_port.c
        ../../src/platform.c
        ../../src/event_manager.c
        ../../src/log.c
        ../../src/topic.c
        ../../third-party/tinyprintf/tinyprintf.c
        $<TARGET_OBJECTS:test_objlib>)
add_dependencies(log_port_test test_objlib cmocka)
target_link_libraries(log_port_test cmocka)
add_test(log_port_test ${CMAKE_CURRENT_BINARY_DIR}/log_port_test)

# waveform_port_test special build to break dependencies
SET_FILENAME("waveform_sink_test.c")
add_executable(waveform_sink_test waveform_sink_test.c
        ../../src/comm/wave_sink_port.c
        ../../src/platform.c
        ../../src/event_manager.c
        ../../src/log.c
        ../../src/cstr.c
        ../../src/topic.c
        $<TARGET_OBJECTS:test_objlib>)
add_dependencies(waveform_sink_test test_objlib cmocka)
target_link_libraries(waveform_sink_test cmocka)
add_test(waveform_sink_test ${CMAKE_CURRENT_BINARY_DIR}/waveform_sink_test)

add_executable(stream_tester stream_tester.c $<TARGET_OBJECTS:fitterbap_objlib>)
add_dependencies(stream_tester fitterbap_objlib test_objlib)

add_executable(length_check length_check.c)

include_directories(../../host/include)

SET_FILENAME("host.c")
add_executable(host host.c $<TARGET_OBJECTS:fitterbap_objlib>)
target_link_libraries(host)
add_dependencies(host fitterbap_objlib test_objlib)

SET_FILENAME("uart_thread_tester.c")
add_executable(uart_thread_tester uart_thread_tester.c $<TARGET_OBJECTS:fitterbap_objlib>)
target_link_libraries(uart_thread_tester)
add_dependencies(uart_thread_tester fitterbap_objlib test_objlib)
