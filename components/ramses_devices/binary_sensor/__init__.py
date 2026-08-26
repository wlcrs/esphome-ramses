import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
)

CONF_ZONE = "zone"
AUTO_LOAD = ["ramses_devices"]
DEPENDENCIES = ["ramses_esp"]

from .. import (
    CONF_CONTROLLER_ADDRESS,
    CONF_DEVICE_ADDRESS,
    CONF_RAMSES_ADDRESS,
    CONF_RAMSES_ESP_ID,
    CONF_ZONE_INDEX,
    RamsesESPComponent,
    ramses_devices_ns,
)

RamsesBinarySensor = ramses_devices_ns.class_(
    "RamsesBinarySensor", binary_sensor.BinarySensor, cg.Component
)
RamsesBinarySensorType = ramses_devices_ns.enum("RamsesBinarySensorType", is_class=True)

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
    binary_sensor.binary_sensor_schema(RamsesBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
            cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True),
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_CONTROLLER_ADDRESS): cv.string,
            cv.Optional(CONF_RAMSES_ADDRESS): cv.string,
            cv.Optional(CONF_ZONE_INDEX): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ZONE): cv.int_range(min=0, max=15),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

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
