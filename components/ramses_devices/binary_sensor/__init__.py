import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import (
    CONF_DEVICE_ADDRESS,
    CONF_RAMSES_ESP_ID,
    RamsesEntityBase,
    ramses_devices_ns,
)

AUTO_LOAD = ["ramses_esp"]
DEPENDENCIES = ["ramses_esp"]

RamsesBinarySensor = ramses_devices_ns.class_(
    "RamsesBinarySensor",
    binary_sensor.BinarySensor,
    cg.Component,
    RamsesEntityBase,
)
RamsesBinarySensorType = ramses_devices_ns.enum("RamsesBinarySensorType")

CONF_ZONE_INDEX = "zone_index"
CONF_TYPE = "type"

BINARY_SENSOR_TYPES = {
    "filter_alarm": RamsesBinarySensorType.FILTER_ALARM,
    "flame_active": RamsesBinarySensorType.FLAME_ACTIVE,
    "fault_alarm": RamsesBinarySensorType.FAULT_ALARM,
    "window_open": RamsesBinarySensorType.WINDOW_OPEN,
    "bypass_active": RamsesBinarySensorType.BYPASS_ACTIVE,
    "battery_low": RamsesBinarySensorType.BATTERY_LOW,
    "actuator_relay": RamsesBinarySensorType.ACTUATOR_RELAY,
}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        RamsesBinarySensor,
    )
    .extend(
        {
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(cg.Component),
            cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_ZONE_INDEX): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[cv.CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    hub = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_sensor_type(config[CONF_TYPE]))

    if CONF_DEVICE_ADDRESS in config:
        cg.add(var.set_device_address(config[CONF_DEVICE_ADDRESS]))
    if CONF_ZONE_INDEX in config:
        cg.add(var.set_zone_index(config[CONF_ZONE_INDEX]))
