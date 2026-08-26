import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CONF_ZONE = "zone"
AUTO_LOAD = ["ramses_devices"]
DEPENDENCIES = ["ramses_esp"]

from .. import (
    CONF_CONTROLLER_ADDRESS,
    CONF_DEVICE_ADDRESS,
    CONF_RAMSES_ADDRESS,
    CONF_RAMSES_ESP_ID,
    CONF_RELAY_INDEX,
    CONF_ZONE_INDEX,
    RamsesESPComponent,
    ramses_devices_ns,
)

RamsesSensor = ramses_devices_ns.class_("RamsesSensor", sensor.Sensor, cg.Component)
RamsesSensorType = ramses_devices_ns.enum("RamsesSensorType", is_class=True)

SENSOR_TYPES = {
    "temperature": RamsesSensorType.ZONE_TEMPERATURE,
    "setpoint": RamsesSensorType.ZONE_SETPOINT,
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
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(RamsesSensor)
    .extend(
        {
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, lower=True),
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_CONTROLLER_ADDRESS): cv.string,
            cv.Optional(CONF_RAMSES_ADDRESS): cv.string,
            cv.Optional(CONF_ZONE_INDEX): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ZONE): cv.int_range(min=0, max=15),
            cv.Optional(CONF_RELAY_INDEX): cv.int_range(min=0, max=255),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    sensor_type = config[CONF_TYPE]
    cg.add(var.set_sensor_type(sensor_type))

    addr = (
        config.get(CONF_DEVICE_ADDRESS)
        or config.get(CONF_CONTROLLER_ADDRESS)
        or config.get(CONF_RAMSES_ADDRESS)
    )
    if addr is not None:
        cg.add(var.set_device_address(addr))

    zone_idx = config.get(CONF_ZONE_INDEX)
    if zone_idx is None:
        zone_idx = config.get(CONF_ZONE)
    if zone_idx is not None:
        cg.add(var.set_zone_index(zone_idx))

    relay_idx = config.get(CONF_RELAY_INDEX)
    if relay_idx is not None:
        cg.add(var.set_relay_index(relay_idx))
