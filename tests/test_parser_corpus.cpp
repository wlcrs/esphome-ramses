#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cassert>
#include "components/ramses_esp/ramses_message.h"
#include "components/ramses_esp/ramses_decoder.h"

namespace fs = std::filesystem;
using namespace esphome::ramses_esp;

static int total_files_processed = 0;
static int total_lines_read = 0;
static int total_packets_parsed = 0;
static int total_opcodes_decoded = 0;

static std::string extract_hgi80_frame(const std::string &line) {
  // Strip comments (#)
  size_t hash_pos = line.find('#');
  std::string cleaned = (hash_pos != std::string::npos) ? line.substr(0, hash_pos) : line;

  // Trim whitespace
  size_t start = cleaned.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = cleaned.find_last_not_of(" \t\r\n");
  cleaned = cleaned.substr(start, end - start + 1);

  if (cleaned.empty()) return "";

  // If line starts with timestamp (e.g. 2024-01-01T12:00:00.000000 or 2024-...), skip the first token
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
  if (!infile.is_open()) return;

  total_files_processed++;
  std::string line;
  while (std::getline(infile, line)) {
    total_lines_read++;
    std::string frame = extract_hgi80_frame(line);
    if (frame.empty() || frame.size() < 20) continue;

    RamsesMessage msg;
    bool ok = msg.from_hgi80(frame);
    if (ok) {
      total_packets_parsed++;

      // Decode known opcodes
      uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
      switch (opcode) {
        case 0x30C9: {
          auto dec = TemperaturePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x2309: {
          auto dec = SetpointPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x1F09: {
          auto dec = SystemSyncPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x0004: {
          auto dec = ZoneNamePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x000C: {
          auto dec = ZoneStructurePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x000A: {
          auto dec = ZoneRolePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x22F1:
        case 0x22F3: {
          auto dec = FanStatePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x10E0: {
          auto dec = DeviceInfoPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x3150: {
          auto dec = HeatDemandPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x1060: {
          auto dec = DeviceBatteryPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x3220: {
          auto dec = OpenThermPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x10D0: {
          auto dec = FilterInfoPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x12C0: {
          auto dec = OutdoorTemperaturePayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x1260: {
          auto dec = DhwStatePayload::decode_temp(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x12F0: {
          auto dec = DhwConfigPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x1F41: {
          auto dec = DhwStatePayload::decode_state(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x0008: {
          auto dec = RelayDemandPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x1298: {
          auto dec = Co2SensorPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x12A0: {
          auto dec = AirQualityPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x12B0: {
          auto dec = ContactSensorPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        case 0x10A0: {
          auto dec = VentilationInfoPayload::decode(msg.payload, msg.n_payload);
          if (dec.has_value()) total_opcodes_decoded++;
          break;
        }
        default:
          break;
      }
    }
  }
}

int main(int argc, char **argv) {
  std::string corpus_dir = "../ramses_rf/tests/tests_rf/data_driven/parsers";
  if (argc > 1) {
    corpus_dir = argv[1];
  }

  std::cout << "====================================================\n";
  std::cout << "Running Bulk Corpus Test against ramses_rf log files\n";
  std::cout << "Target Directory: " << corpus_dir << "\n";
  std::cout << "====================================================\n";

  if (!fs::exists(corpus_dir)) {
    std::cerr << "Directory does not exist: " << corpus_dir << "\n";
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
  std::cout << "  - Valid Packets Parsed:  " << total_packets_parsed << "\n";
  std::cout << "  - Known Opcode Decodes:  " << total_opcodes_decoded << "\n";
  std::cout << "====================================================\n";

  assert(total_files_processed >= 50);
  assert(total_packets_parsed > 1500);

  std::cout << "ALL LOG CORPUS TESTS PASSED!\n";
  return 0;
}
