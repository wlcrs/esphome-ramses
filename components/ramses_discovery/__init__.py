import gzip
import shutil
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

HTML_FILE = Path(__file__).parent / "discovery.html"
if HTML_FILE.exists():
    HTML_BYTES = HTML_FILE.read_bytes()
    HTML_GZ = gzip.compress(HTML_BYTES, mtime=0)
else:
    HTML_GZ = None

try:
    from esphome.components import web_server_base

    CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
    HAS_WEB_SERVER = True
except ImportError:
    HAS_WEB_SERVER = False

AUTO_LOAD = [
    "ramses_esp",
    "sensor",
    "binary_sensor",
    "climate",
    "fan",
    "water_heater",
    "button",
    "json",
]
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
    devices_dir = Path(__file__).resolve().parent.parent / "ramses_devices"
    if devices_dir.exists():
        for dst_sub in (
            "components/ramses_devices",
            "esphome/components/ramses_devices",
        ):
            dst_dir = CORE.relative_src_path(dst_sub)
            dst_dir.mkdir(parents=True, exist_ok=True)
            for ext in ("*.h", "*.cpp"):
                for f in devices_dir.glob(ext):
                    shutil.copy2(f, dst_dir / f.name)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_RAMSES_ESP_ID])
    cg.add(var.set_parent(parent))

    cg.add(var.set_active_probing(config[CONF_ACTIVE_PROBING]))
    cg.add(var.set_probing_interval(config[CONF_PROBING_INTERVAL]))

    cg.add_define("USE_SENSOR")
    cg.add_define("ESPHOME_ENTITY_SENSOR_COUNT", 32)
    cg.add_define("USE_BINARY_SENSOR")
    cg.add_define("ESPHOME_ENTITY_BINARY_SENSOR_COUNT", 16)
    cg.add_define("USE_CLIMATE")
    cg.add_define("ESPHOME_ENTITY_CLIMATE_COUNT", 16)
    cg.add_define("USE_FAN")
    cg.add_define("ESPHOME_ENTITY_FAN_COUNT", 8)
    cg.add_define("USE_WATER_HEATER")
    cg.add_define("ESPHOME_ENTITY_WATER_HEATER_COUNT", 4)
    cg.add_define("USE_BUTTON")
    cg.add_define("ESPHOME_ENTITY_BUTTON_COUNT", 4)

    from esphome.const import (
        DEVICE_CLASS_BATTERY,
        DEVICE_CLASS_CARBON_DIOXIDE,
        DEVICE_CLASS_DURATION,
        DEVICE_CLASS_HUMIDITY,
        DEVICE_CLASS_PROBLEM,
        DEVICE_CLASS_RUNNING,
        DEVICE_CLASS_TEMPERATURE,
        DEVICE_CLASS_WINDOW,
        UNIT_CELSIUS,
        UNIT_MINUTE,
        UNIT_PARTS_PER_MILLION,
        UNIT_PERCENT,
    )
    from esphome.core.entity_helpers import (
        _DC_SHIFT,
        _UOM_SHIFT,
        register_device_class,
        register_unit_of_measurement,
    )

    cg.add_define("USE_ENTITY_DEVICE_CLASS")
    cg.add_define("USE_ENTITY_UNIT_OF_MEASUREMENT")

    uom_c = register_unit_of_measurement(UNIT_CELSIUS)
    uom_pct = register_unit_of_measurement(UNIT_PERCENT)
    uom_ppm = register_unit_of_measurement(UNIT_PARTS_PER_MILLION)
    uom_min = register_unit_of_measurement(UNIT_MINUTE)
    uom_d = register_unit_of_measurement("d")

    dc_temp = register_device_class(DEVICE_CLASS_TEMPERATURE)
    dc_hum = register_device_class(DEVICE_CLASS_HUMIDITY)
    dc_co2 = register_device_class(DEVICE_CLASS_CARBON_DIOXIDE)
    dc_batt = register_device_class(DEVICE_CLASS_BATTERY)
    dc_dur = register_device_class(DEVICE_CLASS_DURATION)
    dc_prob = register_device_class(DEVICE_CLASS_PROBLEM)
    dc_win = register_device_class(DEVICE_CLASS_WINDOW)
    dc_run = register_device_class(DEVICE_CLASS_RUNNING)

    cg.add_define("RAMSES_FIELDS_TEMP", (dc_temp << _DC_SHIFT) | (uom_c << _UOM_SHIFT))
    cg.add_define(
        "RAMSES_FIELDS_HUMIDITY", (dc_hum << _DC_SHIFT) | (uom_pct << _UOM_SHIFT)
    )
    cg.add_define("RAMSES_FIELDS_CO2", (dc_co2 << _DC_SHIFT) | (uom_ppm << _UOM_SHIFT))
    cg.add_define("RAMSES_FIELDS_PERCENT", (0 << _DC_SHIFT) | (uom_pct << _UOM_SHIFT))
    cg.add_define(
        "RAMSES_FIELDS_BATTERY", (dc_batt << _DC_SHIFT) | (uom_pct << _UOM_SHIFT)
    )
    cg.add_define(
        "RAMSES_FIELDS_MINUTES", (dc_dur << _DC_SHIFT) | (uom_min << _UOM_SHIFT)
    )
    cg.add_define("RAMSES_FIELDS_DAYS", (dc_dur << _DC_SHIFT) | (uom_d << _UOM_SHIFT))
    cg.add_define("RAMSES_FIELDS_BIN_PROBLEM", (dc_prob << _DC_SHIFT))
    cg.add_define("RAMSES_FIELDS_BIN_WINDOW", (dc_win << _DC_SHIFT))
    cg.add_define("RAMSES_FIELDS_BIN_RUNNING", (dc_run << _DC_SHIFT))
    cg.add_define("RAMSES_FIELDS_BIN_BATTERY", (dc_batt << _DC_SHIFT))

    if HTML_GZ is not None:
        bytes_str = ", ".join(f"0x{b:02x}" for b in HTML_GZ)
        cg.add_global(
            cg.RawStatement(
                f"static const uint8_t RAMSES_DISCOVERY_HTML_GZ[{len(HTML_GZ)}] = {{{bytes_str}}};"
            )
        )
        cg.add(
            var.set_html_gz(cg.RawExpression("RAMSES_DISCOVERY_HTML_GZ"), len(HTML_GZ))
        )

    if HAS_WEB_SERVER and CONF_WEB_SERVER_BASE_ID in config:
        web_server = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
        cg.add(var.set_web_server_base(web_server))
