import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import CONF_ID

AUTO_LOAD = ["ramses_devices"]
DEPENDENCIES = ["ramses_esp"]

CONF_SCHEME = "scheme"
CONF_FAKE_REMOTE_ADDRESS = "fake_remote_address"
CONF_REMOTE_ADDRESS = "remote_address"

from .. import (
    CONF_DEVICE_ADDRESS,
    CONF_RAMSES_ADDRESS,
    CONF_RAMSES_ESP_ID,
    RamsesESPComponent,
    ramses_devices_ns,
    ramses_esp_ns,
)

RamsesFan = ramses_devices_ns.class_("RamsesFan", fan.Fan, cg.Component)
HvacScheme = ramses_esp_ns.enum("HvacScheme", is_class=True)

HVAC_SCHEMES = {
    "orcon": HvacScheme.ORCON,
    "vasco": HvacScheme.VASCO,
    "itho": HvacScheme.ITHO,
    "zehnder": HvacScheme.ZEHNDER,
}

CONFIG_SCHEMA = (
    fan.fan_schema(RamsesFan)
    .extend(
        {
            cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
            cv.Optional(CONF_DEVICE_ADDRESS): cv.string,
            cv.Optional(CONF_RAMSES_ADDRESS): cv.string,
            cv.Optional(CONF_FAKE_REMOTE_ADDRESS): cv.string,
            cv.Optional(CONF_REMOTE_ADDRESS): cv.string,
            cv.Optional(CONF_SCHEME, default="orcon"): cv.enum(HVAC_SCHEMES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    addr = config.get(CONF_DEVICE_ADDRESS) or config.get(CONF_RAMSES_ADDRESS)
    if addr is not None:
        cg.add(var.set_device_address(addr))

    remote_addr = config.get(CONF_FAKE_REMOTE_ADDRESS) or config.get(CONF_REMOTE_ADDRESS)
    if remote_addr is not None:
        cg.add(var.set_fake_remote_address(remote_addr))

    if CONF_SCHEME in config:
        cg.add(var.set_scheme(config[CONF_SCHEME]))
