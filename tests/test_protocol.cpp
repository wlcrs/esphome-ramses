#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "components/ramses_esp/ramses_codec.h"
#include "components/ramses_esp/ramses_message.h"

using namespace esphome::ramses_esp;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(cond, msg) \
  do { \
    tests_run++; \
    if (cond) { \
      tests_passed++; \
      std::cout << "  [PASS] " << msg << "\n"; \
    } else { \
      std::cerr << "  [FAIL] " << msg << " (Line " << __LINE__ << ")\n"; \
      assert(false); \
    } \
  } while (0)

void test_manchester_codec() {
  std::cout << "Testing Manchester Codec...\n";
  for (uint8_t nibble = 0; nibble < 16; nibble++) {
    uint8_t encoded = manchester_encode(nibble);
    TEST_ASSERT(manchester_code_valid(encoded), "Encoded byte is valid Manchester");
    uint8_t decoded = manchester_decode(encoded);
    TEST_ASSERT(decoded == nibble, "Manchester roundtrip matches original nibble");
  }

  // Test invalid Manchester bytes (e.g. 0x00, 0xFF, 0xAA is valid, 0x01 is invalid)
  TEST_ASSERT(!manchester_code_valid(0x00), "0x00 is invalid Manchester");
  TEST_ASSERT(!manchester_code_valid(0xFF), "0xFF is invalid Manchester");
  TEST_ASSERT(!manchester_code_valid(0x01), "0x01 is invalid Manchester");
  TEST_ASSERT(!manchester_code_valid(0x35), "0x35 trailer is invalid Manchester");
}

void test_ramses_address() {
  std::cout << "\nTesting RAMSES Address Conversion...\n";
  RamsesAddress addr = RamsesAddress::from_string("01:123456");
  TEST_ASSERT(addr.is_valid, "Address '01:123456' is valid");
  TEST_ASSERT(addr.dev_class == 1, "Address dev_class is 1");
  TEST_ASSERT(addr.id == 123456, "Address id is 123456");
  TEST_ASSERT(addr.to_string() == "01:123456", "Address string serialization matches");

  RamsesAddress empty_addr = RamsesAddress::from_string("--:------");
  TEST_ASSERT(!empty_addr.is_valid, "Empty address is invalid");
  TEST_ASSERT(empty_addr.to_string() == "--:------", "Empty address formats as --:------");

  // Bytes roundtrip
  uint8_t bytes[3];
  addr.to_bytes(bytes);
  RamsesAddress addr2 = RamsesAddress::from_bytes(bytes);
  TEST_ASSERT(addr2.dev_class == addr.dev_class, "Byte roundtrip dev_class matches");
  TEST_ASSERT(addr2.id == addr.id, "Byte roundtrip id matches");
}

void test_hgi80_parsing_and_formatting() {
  std::cout << "\nTesting HGI80 Protocol Parsing & Formatting...\n";

  // Real world heating demand / temperature query message (2 spaces before I)
  std::string sample = "045  I --- 01:123456 --:------ 01:123456 1F09 003 0007D0";
  RamsesMessage msg;
  bool ok = msg.from_hgi80(sample);
  TEST_ASSERT(ok, "Parsed valid HGI80 string");
  TEST_ASSERT(msg.type == RAMSES_MSG_I, "Message type is I");
  TEST_ASSERT(msg.rssi == 45, "RSSI parsed as 45");
  TEST_ASSERT(msg.len == 3, "Payload length is 3");
  TEST_ASSERT(msg.payload[0] == 0x00 && msg.payload[1] == 0x07 && msg.payload[2] == 0xD0, "Payload bytes match 0007D0");

  std::string formatted = msg.to_hgi80();
  std::cout << "Original : [" << sample << "]\n";
  std::cout << "Formatted: [" << formatted << "]\n";
  TEST_ASSERT(formatted == sample, "Formatted HGI80 matches original input");

  // Test Request message
  std::string rq_sample = "--- RQ --- 01:054321 01:123456 --:------ 3150 002 0000";
  RamsesMessage rq_msg;
  ok = rq_msg.from_hgi80(rq_sample);
  TEST_ASSERT(ok, "Parsed valid RQ message");
  TEST_ASSERT(rq_msg.type == RAMSES_MSG_RQ, "Message type is RQ");
  TEST_ASSERT(rq_msg.len == 2, "Payload length is 2");
}

void test_raw_frame_construction() {
  std::cout << "\nTesting Raw Radio Frame Construction...\n";
  std::string sample = "--- I --- 01:123456 --:------ 01:123456 1F09 003 0007D0";
  RamsesMessage msg;
  msg.from_hgi80(sample);

  std::vector<uint8_t> raw = msg.to_raw_frame();
  TEST_ASSERT(raw.size() > 30, "Raw frame contains preamble, sync, payload, and trailer");
  // Check preamble
  TEST_ASSERT(raw[0] == 0x55 && raw[1] == 0x55, "Preamble begins with 0x55 0x55");
  // Check sync header (FF 00 33 55 53)
  TEST_ASSERT(raw[20] == 0xFF && raw[21] == 0x00 && raw[22] == 0x33 && raw[23] == 0x55 && raw[24] == 0x53,
              "Sync word is FF 00 33 55 53");
  // Check trailer
  TEST_ASSERT(raw[raw.size() - 2] == 0x35, "Trailer byte is 0x35");
}

int main() {
  std::cout << "========================================\n";
  std::cout << " Running ESPHome RAMSES Native Unit Tests\n";
  std::cout << "========================================\n";

  test_manchester_codec();
  test_ramses_address();
  test_hgi80_parsing_and_formatting();
  test_raw_frame_construction();

  std::cout << "\n========================================\n";
  std::cout << " Test Summary: " << tests_passed << "/" << tests_run << " passed\n";
  std::cout << "========================================\n";
  return (tests_passed == tests_run) ? 0 : 1;
}
