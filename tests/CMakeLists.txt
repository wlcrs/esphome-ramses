cmake_minimum_required(VERSION 3.14)
project(esphome_ramses_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(test_protocol
    test_protocol.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
)

add_executable(test_mock
    test_mock.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
)

add_executable(test_parity_cases
    test_parity_cases.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
)

add_executable(test_opcode_codecs
    test_opcode_codecs.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
)

enable_testing()
add_test(NAME ProtocolTest COMMAND test_protocol)
add_test(NAME MockTest COMMAND test_mock)
add_test(NAME ParityJsonTest COMMAND test_parity_cases "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parity_cases.json")
add_test(NAME OpcodeCodecsTest COMMAND test_opcode_codecs)
