import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from .. import (
    CONF_DEVICE_ADDRESS,
    CONF_RAMSES_ESP_ID,
    RamsesEntityBase,
    ramses_devices_ns,
)

AUTO_LOAD = ["ramses_esp"]
DEPENDENCIES = ["ramses_esp"]

RamsesSensor = ramses_devices_ns.class_(
    "RamsesSensor", sensor.Sensor, cg.Component, RamsesEntityBase
)
RamsesSensorType = ramses_devices_ns.enum("RamsesSensorType")

CONF_ZONE_INDEX = "zone_index"
CONF_RELAY_INDEX = "relay_index"
CONF_TYPE = "type"

SENSOR_TYPES = {
    "zone_temperature": RamsesSensorType.ZONE_TEMPERATURE,
    "zone_setpoint": RamsesSensorType.ZONE_SETPOINT,
    "outdoor_temperature": RamsesSensorType.OUTDOOR_TEMPERATURE,
    "heat_demand": RamsesSensorType.HEAT_DEMAND,
    "relay_demand": RamsesSensorType.RELAY_DEMAND,
    "co2": RamsesSensorType.CO2,
    "indoor_humidity": RamsesSensorType.INDOOR_HUMIDITY,
    "outdoor_humidity": RamsesSensorType.OUTDOOR_HUMIDITY,
    "air_quality_temperature": RamsesSensorType.AIR_QUALITY_TEMPERATURE,
    "bypass_position": RamsesSensorType.BYPASS_POSITION,
    "filter_remaining_days": RamsesSensorType.FILTER_REMAINING_DAYS,
    "filter_lifetime_days": RamsesSensorType.FILTER_LIFETIME_DAYS,
    "filter_remaining_percent": RamsesSensorType.FILTER_REMAINING_PERCENT,
    "opentherm_modulation": RamsesSensorType.OPENTHERM_MODULATION,
    "flow_temperature": RamsesSensorType.OPENTHERM_FLOW_TEMP,
    "return_temperature": RamsesSensorType.OPENTHERM_RETURN_TEMP,
    "battery_level": RamsesSensorType.BATTERY_LEVEL,
    "supply_temperature": RamsesSensorType.SUPPLY_TEMPERATURE,
    "exhaust_temperature": RamsesSensorType.EXHAUST_TEMPERATURE,
    "supply_fan_speed": RamsesSensorType.SUPPLY_FAN_SPEED,
    "exhaust_fan_speed": RamsesSensorType.EXHAUST_FAN_SPEED,
    "remaining_mins": RamsesSensorType.REMAINING_MINS,
    "actuator_modulation": RamsesSensorType.ACTUATOR_MODULATION,
    "ufh_min_temp": RamsesSensorType.UFH_MIN_TEMP,
    "ufh_max_temp": RamsesSensorType.UFH_MAX_TEMP,
    "spider_temperature": RamsesSensorType.SPIDER_TEMPERATURE,
    "fault_code": RamsesSensorType.FAULT_CODE,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        RamsesSensor,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(cg.Component),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_ZONE_INDEX): cv.int_range(min=0, max=15),
            cv.Optional(CONF_RELAY_INDEX): cv.int_range(min=0, max=7),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    hub = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_sensor_type(config[CONF_TYPE]))

    if CONF_DEVICE_ADDRESS in config:
        cg.add(var.set_device_address(config[CONF_DEVICE_ADDRESS]))
    if CONF_ZONE_INDEX in config:
        cg.add(var.set_zone_index(config[CONF_ZONE_INDEX]))
    if CONF_RELAY_INDEX in config:
        cg.add(var.set_relay_index(config[CONF_RELAY_INDEX]))
