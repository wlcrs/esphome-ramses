import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@wlcrs"]
DEPENDENCIES = ["ramses_esp"]

ramses_devices_ns = cg.esphome_ns.namespace("ramses_devices")
ramses_esp_ns = cg.esphome_ns.namespace("ramses_esp")

RamsesESPComponent = ramses_esp_ns.class_("RamsesESPComponent", cg.Component)

CONF_RAMSES_ESP_ID = "ramses_esp_id"
CONF_DEVICE_ADDRESS = "device_address"
CONF_CONTROLLER_ADDRESS = "controller_address"
CONF_RAMSES_ADDRESS = "ramses_address"
CONF_ZONE_INDEX = "zone_index"

CONFIG_SCHEMA = cv.Schema({})
