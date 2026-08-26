import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_DATA
from esphome.components import cc1101
from components import ramses_esp

DEPENDENCIES = ["esp32", "cc1101", "ramses_esp"]

cc1101_multiplexer_ns = cg.esphome_ns.namespace("cc1101_multiplexer")
CC1101MultiplexerComponent = cc1101_multiplexer_ns.class_(
    "CC1101MultiplexerComponent", cg.Component
)

SendPacketAction = cc1101_multiplexer_ns.class_(
    "SendPacketAction", automation.Action
)

CONF_RAMSES_ID = "ramses_id"
CONF_CC1101_ID = "cc1101_id"
CONF_RX_WINDOW = "rx_window"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CC1101MultiplexerComponent),
        cv.Required(CONF_RAMSES_ID): cv.use_id(ramses_esp.RamsesESPComponent),
        cv.Required(CONF_CC1101_ID): cv.use_id(cc1101.CC1101Component),
        cv.Optional(
            CONF_RX_WINDOW, default="75ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ramses_var = await cg.get_variable(config[CONF_RAMSES_ID])
    cg.add(var.set_ramses(ramses_var))

    cc1101_var = await cg.get_variable(config[CONF_CC1101_ID])
    cg.add(var.set_cc1101(cc1101_var))

    cg.add(var.set_rx_window_ms(config[CONF_RX_WINDOW]))


# Action: cc1101_multiplexer.send_packet
CC1101_MULTIPLEXER_SEND_PACKET_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(CC1101MultiplexerComponent),
        cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
    }
)


@automation.register_action(
    "cc1101_multiplexer.send_packet",
    SendPacketAction,
    CC1101_MULTIPLEXER_SEND_PACKET_SCHEMA,
)
async def cc1101_multiplexer_send_packet_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_data = await cg.templatable(
        config[CONF_DATA], args, cg.std_vector.template(cg.uint8)
    )
    cg.add(var.set_data(template_data))
    return var
