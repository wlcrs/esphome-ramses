from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

HTML_FILE = Path(__file__).parent / "discovery.html"
HTML_CONTENT = HTML_FILE.read_text(encoding="utf-8") if HTML_FILE.exists() else None

try:
    from esphome.components import web_server_base

    CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
    HAS_WEB_SERVER = True
except ImportError:
    HAS_WEB_SERVER = False

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

schema_dict = {
    cv.GenerateID(): cv.declare_id(RamsesDiscoveryComponent),
    cv.GenerateID(CONF_RAMSES_ESP_ID): cv.use_id(RamsesESPComponent),
    cv.Optional(CONF_ACTIVE_PROBING, default=True): cv.boolean,
    cv.Optional(
        CONF_PROBING_INTERVAL, default="30s"
    ): cv.positive_time_period_milliseconds,
}

if HAS_WEB_SERVER:
    schema_dict[cv.Optional(CONF_WEB_SERVER_BASE_ID)] = cv.use_id(
        web_server_base.WebServerBase
    )

CONFIG_SCHEMA = cv.Schema(schema_dict).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    cg.add(var.set_active_probing(config[CONF_ACTIVE_PROBING]))
    cg.add(var.set_probing_interval(config[CONF_PROBING_INTERVAL]))

    if HTML_CONTENT is not None:
        cg.add(var.set_html(cg.RawExpression(f'R"rawhtml({HTML_CONTENT})rawhtml"')))

    if HAS_WEB_SERVER and CONF_WEB_SERVER_BASE_ID in config:
        web_server = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
        cg.add(var.set_web_server_base(web_server))
