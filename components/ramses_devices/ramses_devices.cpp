#include "ramses_devices.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <cmath>

#ifdef USE_ESP_IDF
#include "components/ramses_esp/ramses_esp.h"
#endif

namespace esphome {
namespace ramses_devices {

static const char *const TAG = "ramses_devices";

#ifdef USE_SENSOR
struct SensorOpcodeHandler {
  uint16_t opcode;
  void (*decode)(const ramses_esp::RamsesMessage &msg, RamsesSensor &sensor);
};

struct SensorSpec {
  SensorOpcodeHandler handlers[4];
  uint8_t handler_count;
  uint8_t accuracy_decimals;
};

static void decode_zone_temp(const ramses_esp::RamsesMessage &msg,
                             RamsesSensor &s) {
  auto dec = ramses_esp::TemperaturePayload::decode(msg.payload, msg.n_payload);
  if (dec.has_value()) {
    for (const auto &item : dec->zones) {
      if (!s.get_zone_index().has_value() ||
          item.zone_index == *s.get_zone_index()) {
        if (item.is_valid)
          s.publish_state(item.temperature);
        break;
      }
    }
  }
}

static void decode_zone_setpoint(const ramses_esp::RamsesMessage &msg,
                                 RamsesSensor &s) {
  auto dec = ramses_esp::SetpointPayload::decode(msg.payload, msg.n_payload);
  if (dec.has_value()) {
    for (const auto &item : dec->zones) {
      if (!s.get_zone_index().has_value() ||
          item.zone_index == *s.get_zone_index()) {
        if (item.is_valid)
          s.publish_state(item.setpoint);
        break;
      }
    }
  }
}

static void decode_relay_demand(const ramses_esp::RamsesMessage &msg,
                                RamsesSensor &s) {
  auto dec = ramses_esp::RelayDemandPayload::decode(msg.payload, msg.n_payload);
  auto relay_idx = s.get_relay_index();
  if (dec.has_value() &&
      (!relay_idx.has_value() || dec->relay_index == *relay_idx)) {
    s.publish_state(dec->demand_percent);
  }
}

static void
decode_ventilation_bypass_position(const ramses_esp::RamsesMessage &msg,
                                   RamsesSensor &s) {
  auto dec =
      ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.n_payload);
  if (dec.has_value())
    s.publish_state(dec->bypass_position);
}

static void decode_actuator_modulation(const ramses_esp::RamsesMessage &msg,
                                       RamsesSensor &s) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  auto dec = ramses_esp::ActuatorStatePayload::decode(msg.payload,
                                                      msg.n_payload, opcode);
  if (dec.has_value())
    s.publish_state(dec->modulation_percent);
}

static void decode_ufh_min_temp(const ramses_esp::RamsesMessage &msg,
                                RamsesSensor &s) {
  auto dec =
      ramses_esp::UfhSetpointBoundsPayload::decode(msg.payload, msg.n_payload);
  auto zone_idx = s.get_zone_index();
  if (dec.has_value() && dec->min_temp.has_value()) {
    if (!zone_idx.has_value() || dec->ufh_index == *zone_idx)
      s.publish_state(*dec->min_temp);
  }
}

static void decode_ufh_max_temp(const ramses_esp::RamsesMessage &msg,
                                RamsesSensor &s) {
  auto dec =
      ramses_esp::UfhSetpointBoundsPayload::decode(msg.payload, msg.n_payload);
  auto zone_idx = s.get_zone_index();
  if (dec.has_value() && dec->max_temp.has_value()) {
    if (!zone_idx.has_value() || dec->ufh_index == *zone_idx)
      s.publish_state(*dec->max_temp);
  }
}

static void decode_spider_temp(const ramses_esp::RamsesMessage &msg,
                               RamsesSensor &s) {
  auto dec =
      ramses_esp::SpiderTemperaturesPayload::decode(msg.payload, msg.n_payload);
  if (dec.has_value() && dec->primary_temp.has_value())
    s.publish_state(*dec->primary_temp);
}

static void decode_fault_code(const ramses_esp::RamsesMessage &msg,
                              RamsesSensor &s) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  auto dec = ramses_esp::SystemFaultLogPayload::decode(msg.payload,
                                                       msg.n_payload, opcode);
  if (dec.has_value())
    s.publish_state(dec->fault_code);
}

static const SensorSpec SENSOR_SPECS[] =
    {
        // ZONE_TEMPERATURE
        {{{0x30C9, decode_zone_temp}}, 1, 1},
        // ZONE_SETPOINT
        {{{0x2309, decode_zone_setpoint}}, 1, 1},
        // OUTDOOR_TEMPERATURE
        {{{0x12C0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::OutdoorTemperaturePayload::decode(
                 msg.payload, msg.n_payload);
             if (dec.has_value() && dec->is_valid)
               s.publish_state(dec->temperature);
           }},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->outdoor_temp.has_value())
               s.publish_state(*dec->outdoor_temp);
           }}},
         2,
         1},
        // HEAT_DEMAND
        {{{0x3150,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HeatDemandPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value()) {
               auto zone_idx = s.get_zone_index();
               if (!zone_idx.has_value() ||
                   dec->domain_or_zone_index == *zone_idx) {
                 s.publish_state(dec->demand_percent);
               }
             }
           }},
          {0x0008, decode_relay_demand}},
         2,
         0},
        // RELAY_DEMAND
        {{{0x0008, decode_relay_demand}}, 1, 0},
        // CO2
        {{{0x1298,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::Co2SensorPayload::decode(msg.payload,
                                                             msg.n_payload);
             if (dec.has_value() && dec->is_valid)
               s.publish_state(dec->co2_ppm);
           }},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->co2_ppm.has_value())
               s.publish_state(*dec->co2_ppm);
           }}},
         2,
         0},
        // INDOOR_HUMIDITY
        {{{0x12A0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::AirQualityPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value() && dec->sensor_index == 0 &&
                 dec->humidity.has_value()) {
               s.publish_state(*dec->humidity);
             }
           }},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->indoor_humidity.has_value())
               s.publish_state(*dec->indoor_humidity);
           }}},
         2,
         0},
        // OUTDOOR_HUMIDITY
        {{{0x12A0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::AirQualityPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value() && dec->sensor_index == 2 &&
                 dec->humidity.has_value()) {
               s.publish_state(*dec->humidity);
             }
           }},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->outdoor_humidity.has_value())
               s.publish_state(*dec->outdoor_humidity);
           }}},
         2,
         0},
        // AIR_QUALITY_TEMPERATURE
        {{{0x12A0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::AirQualityPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value() && dec->temperature.has_value())
               s.publish_state(*dec->temperature);
           }},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->indoor_temp.has_value())
               s.publish_state(*dec->indoor_temp);
           }}},
         2,
         1},
        // BYPASS_POSITION
        {{{0x10A0, decode_ventilation_bypass_position},
          {0x22E5, decode_ventilation_bypass_position},
          {0x31DA,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::HvacTelemetryPayload::decode(msg.payload,
                                                                 msg.n_payload);
             if (dec.has_value() && dec->bypass_position.has_value())
               s.publish_state(*dec->bypass_position);
           }}},
         3,
         0},
        // FILTER_REMAINING_DAYS
        {{{0x10D0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value())
               s.publish_state(dec->remaining_days);
           }}},
         1,
         0},
        // FILTER_LIFETIME_DAYS
        {{{0x10D0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value())
               s.publish_state(dec->lifetime_days);
           }}},
         1,
         0},
        // FILTER_REMAINING_PERCENT
        {{{0x10D0,
           [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
             auto dec = ramses_esp::FilterInfoPayload::decode(msg.payload,
                                                              msg.n_payload);
             if (dec.has_value())
               s.publish_state(dec->remaining_percent);
           }}},
         1,
         0},
        // OPENTHERM_MODULATION
        {
            {{0x3220,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::OpenThermPayload::decode(msg.payload,
                                                                msg.n_payload);
                if (dec.has_value())
                  s.publish_state(dec->modulation_percent);
              }}},
            1,
            0},
        // OPENTHERM_FLOW_TEMP
        {
            {{0x3220,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::OpenThermPayload::decode(msg.payload,
                                                                msg.n_payload);
                if (dec.has_value() && dec->flow_temp.has_value())
                  s.publish_state(*dec->flow_temp);
              }}},
            1,
            1},
        // OPENTHERM_RETURN_TEMP
        {
            {{0x3220,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::OpenThermPayload::decode(msg.payload,
                                                                msg.n_payload);
                if (dec.has_value() && dec->return_temp.has_value())
                  s.publish_state(*dec->return_temp);
              }}},
            1,
            1},
        // BATTERY_LEVEL
        {
            {{0x1060,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::DeviceBatteryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value())
                  s.publish_state(dec->battery_percent);
              }}},
            1,
            0},
        // SUPPLY_TEMPERATURE
        {
            {{0x31DA,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::HvacTelemetryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value() && dec->supply_temp.has_value())
                  s.publish_state(*dec->supply_temp);
              }}},
            1,
            1},
        // EXHAUST_TEMPERATURE
        {
            {{0x31DA,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::HvacTelemetryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value() && dec->exhaust_temp.has_value())
                  s.publish_state(*dec->exhaust_temp);
              }}},
            1,
            1},
        // SUPPLY_FAN_SPEED
        {
            {{0x31DA,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::HvacTelemetryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value() && dec->supply_fan_speed.has_value())
                  s.publish_state(*dec->supply_fan_speed);
              }}},
            1,
            0},
        // EXHAUST_FAN_SPEED
        {
            {{0x31DA,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::HvacTelemetryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value() && dec->exhaust_fan_speed.has_value())
                  s.publish_state(*dec->exhaust_fan_speed);
              }}},
            1,
            0},
        // REMAINING_MINS
        {
            {{0x31DA,
              [](const ramses_esp::RamsesMessage &msg, RamsesSensor &s) {
                auto dec = ramses_esp::HvacTelemetryPayload::decode(
                    msg.payload, msg.n_payload);
                if (dec.has_value() && dec->remaining_mins.has_value())
                  s.publish_state(*dec->remaining_mins);
              }}},
            1,
            0},
        // ACTUATOR_MODULATION
        {{{0x3EF0, decode_actuator_modulation},
          {0x3EF1, decode_actuator_modulation},
          {0x3B00, decode_actuator_modulation}},
         3,
         0},
        // UFH_MIN_TEMP
        {{{0x22C9, decode_ufh_min_temp}, {0x2209, decode_ufh_min_temp}}, 2, 1},
        // UFH_MAX_TEMP
        {{{0x22C9, decode_ufh_max_temp}, {0x2209, decode_ufh_max_temp}}, 2, 1},
        // SPIDER_TEMPERATURE
        {{{0x4E01, decode_spider_temp}, {0x4E02, decode_spider_temp}}, 2, 1},
        // FAULT_CODE
        {{{0x0418, decode_fault_code},
          {0x042F, decode_fault_code},
          {0x0009, decode_fault_code},
          {0x4401, decode_fault_code}},
         4,
         0},
};

bool RamsesSensor::matches_opcode(uint16_t opcode) const {
  const auto &spec = SENSOR_SPECS[static_cast<size_t>(this->sensor_type_)];
  for (uint8_t i = 0; i < spec.handler_count; ++i) {
    if (spec.handlers[i].opcode == opcode)
      return true;
  }
  return false;
}

void RamsesSensor::handle_message(const ramses_esp::RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  const auto &spec = SENSOR_SPECS[static_cast<size_t>(this->sensor_type_)];
  for (uint8_t i = 0; i < spec.handler_count; ++i) {
    if (spec.handlers[i].opcode == opcode) {
      spec.handlers[i].decode(msg, *this);
      return;
    }
  }
}

void RamsesSensor::setup() {
  this->setup_base();
  if (!this->has_accuracy_decimals()) {
    this->set_accuracy_decimals(
        SENSOR_SPECS[static_cast<size_t>(this->sensor_type_)]
            .accuracy_decimals);
  }
  if (this->get_state_class() == sensor::STATE_CLASS_NONE) {
    this->set_state_class(sensor::STATE_CLASS_MEASUREMENT);
  }
}

void RamsesSensor::publish_state(float state) {
  int8_t decimals = this->get_accuracy_decimals();
  if (decimals >= 0 && decimals <= 4) {
    float factor = 1.0f;
    for (int i = 0; i < decimals; i++)
      factor *= 10.0f;
    state = std::round(state * factor) / factor;
  }
  sensor::Sensor::publish_state(state);
}
#endif // USE_SENSOR

#ifdef USE_BINARY_SENSOR
struct BinarySensorOpcodeHandler {
  uint16_t opcode;
  void (*decode)(const ramses_esp::RamsesMessage &msg,
                 RamsesBinarySensor &sensor);
};

struct BinarySensorSpec {
  BinarySensorOpcodeHandler handlers[4];
  uint8_t handler_count;
};

static void decode_system_fault_log(const ramses_esp::RamsesMessage &msg,
                                    RamsesBinarySensor &bs) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  auto dec = ramses_esp::SystemFaultLogPayload::decode(msg.payload,
                                                       msg.n_payload, opcode);
  if (dec.has_value())
    bs.publish_state(dec->is_fault);
}

static void
decode_ventilation_bypass_active(const ramses_esp::RamsesMessage &msg,
                                 RamsesBinarySensor &bs) {
  auto dec =
      ramses_esp::VentilationInfoPayload::decode(msg.payload, msg.n_payload);
  if (dec.has_value())
    bs.publish_state(dec->bypass_active);
}

static void decode_actuator_relay(const ramses_esp::RamsesMessage &msg,
                                  RamsesBinarySensor &bs) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  auto dec = ramses_esp::ActuatorStatePayload::decode(msg.payload,
                                                      msg.n_payload, opcode);
  if (dec.has_value())
    bs.publish_state(dec->relay_active);
}

static const BinarySensorSpec BINARY_SENSOR_SPECS[] = {
    // FILTER_ALARM
    {{{0x10A0,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec = ramses_esp::VentilationInfoPayload::decode(msg.payload,
                                                               msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->filter_dirty);
       }},
      {0x10D0,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec =
             ramses_esp::FilterInfoPayload::decode(msg.payload, msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->remaining_days == 0);
       }},
      {0x31D9,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec =
             ramses_esp::HvacFanInfoPayload::decode(msg.payload, msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->filter_dirty);
       }}},
     3},
    // FLAME_ACTIVE
    {{{0x3220,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec =
             ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->flame_active);
       }}},
     1},
    // FAULT_ALARM
    {{{0x3220,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec =
             ramses_esp::OpenThermPayload::decode(msg.payload, msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->fault_active);
       }},
      {0x0418, decode_system_fault_log},
      {0x0009, decode_system_fault_log},
      {0x4401, decode_system_fault_log}},
     4},
    // WINDOW_OPEN
    {{{0x12B0,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec =
             ramses_esp::WindowStatePayload::decode(msg.payload, msg.n_payload);
         if (dec.has_value()) {
           if (!bs.get_zone_index().has_value() ||
               dec->zone_index == *bs.get_zone_index()) {
             bs.publish_state(dec->window_open);
           }
         }
       }}},
     1},
    // BYPASS_ACTIVE
    {{{0x10A0, decode_ventilation_bypass_active},
      {0x22E5, decode_ventilation_bypass_active}},
     2},
    // BATTERY_LOW
    {{{0x1060,
       [](const ramses_esp::RamsesMessage &msg, RamsesBinarySensor &bs) {
         auto dec = ramses_esp::DeviceBatteryPayload::decode(msg.payload,
                                                             msg.n_payload);
         if (dec.has_value())
           bs.publish_state(dec->battery_low);
       }}},
     1},
    // ACTUATOR_RELAY
    {{{0x3EF0, decode_actuator_relay},
      {0x3EF1, decode_actuator_relay},
      {0x3B00, decode_actuator_relay}},
     3},
};

void RamsesBinarySensor::setup() { this->setup_base(); }

bool RamsesBinarySensor::matches_opcode(uint16_t opcode) const {
  const auto &spec =
      BINARY_SENSOR_SPECS[static_cast<size_t>(this->sensor_type_)];
  for (uint8_t i = 0; i < spec.handler_count; ++i) {
    if (spec.handlers[i].opcode == opcode)
      return true;
  }
  return false;
}

void RamsesBinarySensor::handle_message(const ramses_esp::RamsesMessage &msg) {
  uint16_t opcode = ((uint16_t)msg.opcode[0] << 8) | msg.opcode[1];
  const auto &spec =
      BINARY_SENSOR_SPECS[static_cast<size_t>(this->sensor_type_)];
  for (uint8_t i = 0; i < spec.handler_count; ++i) {
    if (spec.handlers[i].opcode == opcode) {
      spec.handlers[i].decode(msg, *this);
      return;
    }
  }
}
#endif // USE_BINARY_SENSOR

} // namespace ramses_devices
} // namespace esphome
