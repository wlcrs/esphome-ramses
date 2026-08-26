cmake_minimum_required(VERSION 3.14)
project(esphome_ramses_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(test_protocol
    test_protocol.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
)

enable_testing()
add_test(NAME ProtocolTest COMMAND test_protocol)
