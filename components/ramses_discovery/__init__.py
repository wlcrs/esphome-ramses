import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["ramses_esp"]
DEPENDENCIES = ["ramses_esp"]

ramses_esp_ns = cg.esphome_ns.namespace("ramses_esp")
RamsesESPComponent = ramses_esp_ns.class_("RamsesESPComponent", cg.Component)

ramses_discovery_ns = cg.esphome_ns.namespace("ramses_discovery")
RamsesDiscoveryComponent = ramses_discovery_ns.class_(
    "RamsesDiscoveryComponent", cg.Component
)

CONF_RAMSES_ESP_ID = "ramses_esp_id"
CONF_ACTIVE_PROBING = "active_probing"
CONF_PROBING_INTERVAL = "probing_interval"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RamsesDiscoveryComponent),
        cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
        cv.Optional(CONF_ACTIVE_PROBING, default=True): cv.boolean,
        cv.Optional(
            CONF_PROBING_INTERVAL, default="30s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    cg.add(var.set_active_probing(config[CONF_ACTIVE_PROBING]))
    cg.add(var.set_probing_interval(config[CONF_PROBING_INTERVAL]))
