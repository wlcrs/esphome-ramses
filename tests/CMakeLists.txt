cmake_minimum_required(VERSION 3.14)
project(esphome_ramses_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
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

add_executable(test_payload_structure
    test_payload_structure.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
)

add_executable(test_devices_sensors
    test_devices_sensors.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
    ../components/ramses_devices/ramses_devices.cpp
)

add_executable(test_devices_climate
    test_devices_climate.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
    ../components/ramses_devices/ramses_climate.cpp
)

add_executable(test_devices_fan
    test_devices_fan.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
    ../components/ramses_devices/ramses_fan.cpp
)

add_executable(test_devices_water_heater
    test_devices_water_heater.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
    ../components/ramses_devices/ramses_water_heater.cpp
)

add_executable(test_parser_corpus
    test_parser_corpus.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
)

add_executable(test_discovery
    test_discovery.cpp
    mock/mock_ramses_esp.cpp
    ../components/ramses_esp/ramses_codec.cpp
    ../components/ramses_esp/ramses_message.cpp
    ../components/ramses_esp/ramses_decoder.cpp
    ../components/ramses_discovery/ramses_discovery.cpp
)

enable_testing()
add_test(NAME ProtocolTest COMMAND test_protocol)
add_test(NAME MockTest COMMAND test_mock)
add_test(NAME ParityJsonTest COMMAND test_parity_cases "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parity_cases.json")
add_test(NAME OpcodeCodecsTest COMMAND test_opcode_codecs)
add_test(NAME PayloadStructureTest COMMAND test_payload_structure)
add_test(NAME DevicesSensorsTest COMMAND test_devices_sensors)
add_test(NAME DevicesClimateTest COMMAND test_devices_climate)
add_test(NAME DevicesFanTest COMMAND test_devices_fan)
add_test(NAME DevicesWaterHeaterTest COMMAND test_devices_water_heater)
add_test(NAME ParserCorpusTest COMMAND test_parser_corpus "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/corpus")
add_test(NAME DiscoveryTest COMMAND test_discovery)
