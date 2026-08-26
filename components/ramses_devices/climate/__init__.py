import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID

CONF_ZONE = "zone"
CONF_ZONE_NAME = "zone_name"
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

RamsesClimate = ramses_devices_ns.class_("RamsesClimate", climate.Climate, cg.Component)

CONFIG_SCHEMA = (
    climate.climate_schema(RamsesClimate)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(RamsesClimate),
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
            cv.Optional(CONF_CONTROLLER_ADDRESS): cv.string,
            cv.Optional(CONF_RAMSES_ADDRESS): cv.string,
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_ZONE_INDEX, default=0): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ZONE): cv.int_range(min=0, max=15),
            cv.Optional(CONF_ZONE_NAME): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    addr = (
        config.get(CONF_CONTROLLER_ADDRESS)
        or config.get(CONF_RAMSES_ADDRESS)
        or config.get(CONF_DEVICE_ADDRESS)
    )
    if addr is not None:
        cg.add(var.set_controller_address(addr))

    zone_idx = config.get(CONF_ZONE_INDEX)
    if zone_idx is None:
        zone_idx = config.get(CONF_ZONE, 0)
    cg.add(var.set_zone_index(zone_idx))

    if CONF_ZONE_NAME in config:
        cg.add(var.set_zone_name(config[CONF_ZONE_NAME]))
