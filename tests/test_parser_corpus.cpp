#include "components/ramses_esp/ramses_decoder.h"
#include "components/ramses_esp/ramses_message.h"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace esphome::ramses_esp;

static int total_files_processed = 0;
static int total_lines_read = 0;
static int total_candidate_frames = 0;
static int total_invalid_packets = 0;
static int total_packets_parsed = 0;
static int total_opcodes_decoded = 0;
static int total_decode_failures = 0;
static std::map<uint16_t, std::pair<int, int>> opcode_counts;
static std::map<uint16_t, std::string> decode_failure_examples;

static void record_decode(uint16_t opcode, bool decoded,
                          const std::string &frame) {
  auto &counts = opcode_counts[opcode];
  counts.first++;
  if (decoded) {
    counts.second++;
    total_opcodes_decoded++;
  } else {
    total_decode_failures++;
    if (decode_failure_examples.find(opcode) == decode_failure_examples.end()) {
      decode_failure_examples[opcode] = frame;
    }
  }
}

static std::string extract_hgi80_frame(const std::string &line) {
  // Strip comments (#)
  size_t hash_pos = line.find('#');
  std::string cleaned =
      (hash_pos != std::string::npos) ? line.substr(0, hash_pos) : line;

  // Trim whitespace
  size_t start = cleaned.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = cleaned.find_last_not_of(" \t\r\n");
  cleaned = cleaned.substr(start, end - start + 1);

  if (cleaned.empty())
    return "";
  if (cleaned.find('*') != std::string::npos)
    return "";

  // If line starts with timestamp (e.g. 2024-01-01T12:00:00.000000 or
  // 2024-...), skip the first token
  if (cleaned.size() > 27 && (cleaned[4] == '-' || cleaned[10] == 'T')) {
    size_t space_pos = cleaned.find(' ');
    if (space_pos != std::string::npos) {
      cleaned = cleaned.substr(space_pos + 1);
      size_t s2 = cleaned.find_first_not_of(" \t\r\n");
      if (s2 != std::string::npos) {
        cleaned = cleaned.substr(s2);
      }
    }
  }

  return cleaned;
}

void test_corpus_file(const fs::path &file_path) {
  std::ifstream infile(file_path);
  if (!infile.is_open())
    return;

  total_files_processed++;
  std::string line;
  while (std::getline(infile, line)) {
    total_lines_read++;
    std::string frame = extract_hgi80_frame(line);
    if (frame.empty() || frame.size() < 20)
      continue;
    total_candidate_frames++;

    RamsesMessage msg;
    bool ok = msg.from_hgi80(frame);
    if (!ok) {
      total_invalid_packets++;
      continue;
    }

    total_packets_parsed++;

    // Decode known opcodes
    uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
    if (msg.type == RAMSES_MSG_RQ || msg.type == RAMSES_MSG_W)
      continue;
    switch (opcode) {
    case 0x30C9: {
      auto dec = TemperaturePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x2309: {
      auto dec = SetpointPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x1F09: {
      auto dec = SystemSyncPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x2E04: {
      auto dec = SystemModePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x0004: {
      auto dec = ZoneNamePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x0005: {
      auto dec = ZoneStructurePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x000C: {
      auto dec = ZoneRolePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x22F1: {
      auto dec = FanStatePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x22F3: {
      auto dec = FanBoostPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x22E5: {
      auto dec = VentilationInfoPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x10E0: {
      auto dec = DeviceInfoPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x3150: {
      auto dec = HeatDemandPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x1060: {
      auto dec = DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x3220: {
      auto dec = OpenThermPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x10D0: {
      auto dec = FilterInfoPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x12C0: {
      auto dec = OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x1260: {
      auto dec = DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x12F0: {
      auto dec = DhwConfigPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x1F41: {
      auto dec = DhwStatePayload::decode_state(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x0008: {
      auto dec = RelayDemandPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x1298: {
      auto dec = Co2SensorPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x12A0: {
      auto dec = AirQualityPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x12B0: {
      auto dec = ContactSensorPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    case 0x10A0: {
      auto dec = VentilationInfoPayload::decode(msg.payload, msg.n_payload);
      record_decode(opcode, dec.has_value(), frame);
      break;
    }
    default:
      break;
    }
  }
}

int main(int argc, char **argv) {
  std::string corpus_dir = "";
  if (argc > 1) {
    corpus_dir = argv[1];
  } else {
    std::vector<std::string> search_paths = {
        "fixtures/corpus", "../fixtures/corpus", "tests/fixtures/corpus",
        "../../tests/fixtures/corpus", "../ramses_rf/tests/tests_rf"};
    for (const auto &p : search_paths) {
      if (fs::exists(p) && fs::is_directory(p)) {
        corpus_dir = p;
        break;
      }
    }
  }

  std::cout << "====================================================\n";
  std::cout << "Running Bulk Corpus Test against RAMSES log files\n";
  std::cout << "Target Directory: " << corpus_dir << "\n";
  std::cout << "====================================================\n";

  if (corpus_dir.empty() || !fs::exists(corpus_dir)) {
    std::cerr << "Corpus directory not found: " << corpus_dir << "\n";
    return 1;
  }

  std::vector<fs::path> log_files;
  for (const auto &entry : fs::recursive_directory_iterator(corpus_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".log") {
      log_files.push_back(entry.path());
    }
  }

  std::sort(log_files.begin(), log_files.end());

  for (const auto &p : log_files) {
    test_corpus_file(p);
  }

  std::cout << "\n====================================================\n";
  std::cout << "Corpus Test Results:\n";
  std::cout << "  - Total Files Processed: " << total_files_processed << "\n";
  std::cout << "  - Total Lines Read:      " << total_lines_read << "\n";
  std::cout << "  - Candidate Frames:      " << total_candidate_frames << "\n";
  std::cout << "  - Invalid Packets:       " << total_invalid_packets << "\n";
  std::cout << "  - Valid Packets Parsed:  " << total_packets_parsed << "\n";
  std::cout << "  - Known Opcode Decodes:  " << total_opcodes_decoded << "\n";
  std::cout << "  - Decode Failures:       " << total_decode_failures << "\n";
  std::cout << "\nKnown opcode coverage:\n";
  const std::vector<uint16_t> supported_opcodes = {
      0x0004, 0x0005, 0x0008, 0x000C, 0x10A0, 0x10D0, 0x10E0, 0x1060,
      0x1260, 0x1298, 0x12A0, 0x12B0, 0x12C0, 0x12F0, 0x1F09, 0x1F41,
      0x22E5, 0x22F1, 0x22F3, 0x2309, 0x2E04, 0x30C9, 0x3150, 0x3220,
  };
  bool all_supported_opcodes_seen = true;
  for (const auto &[opcode, counts] : opcode_counts) {
    char opcode_text[5];
    snprintf(opcode_text, sizeof(opcode_text), "%04X", opcode);
    std::cout << "  - " << opcode_text << ": " << counts.first << " seen, "
              << counts.second << " decoded";
    auto example = decode_failure_examples.find(opcode);
    if (example != decode_failure_examples.end())
      std::cout << "; example: " << example->second;
    std::cout << "\n";
  }
  for (uint16_t opcode : supported_opcodes) {
    if (opcode_counts.find(opcode) == opcode_counts.end()) {
      char opcode_text[5];
      snprintf(opcode_text, sizeof(opcode_text), "%04X", opcode);
      std::cerr << "Missing supported opcode from corpus: " << opcode_text
                << "\n";
      all_supported_opcodes_seen = false;
    }
  }
  std::cout << "====================================================\n";

  if (total_decode_failures > 0) {
    std::cerr
        << "Known opcode payloads failed to decode; see coverage above.\n";
    return 1;
  }
  if (!all_supported_opcodes_seen) {
    return 1;
  }

  assert(total_files_processed >= 50);
  assert(total_packets_parsed > 1500);

  std::cout << "ALL LOG CORPUS TESTS PASSED!\n";
  return 0;
}
