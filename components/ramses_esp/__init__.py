import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_ON_MESSAGE,
    CONF_TRIGGER_ID,
)

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network"]

ramses_esp_ns = cg.esphome_ns.namespace("ramses_esp")
RamsesESPComponent = ramses_esp_ns.class_("RamsesESPComponent", cg.Component)

# Actions
SendHgi80Action = ramses_esp_ns.class_("SendHgi80Action", automation.Action)

# Triggers
RamsesMessageTrigger = ramses_esp_ns.class_(
    "RamsesMessageTrigger", automation.Trigger.template(cg.std_string)
)

CONF_CS_PIN = "cs_pin"
CONF_SCK_PIN = "sck_pin"
CONF_MOSI_PIN = "mosi_pin"
CONF_MISO_PIN = "miso_pin"
CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_UART_NUM = "uart_num"
CONF_COMMAND = "command"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RamsesESPComponent),
        cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_SCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_MISO_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_GDO0_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_GDO2_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_UART_NUM, default=1): cv.int_range(min=0, max=2),
        cv.Optional(CONF_PORT, default=6638): cv.port,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RamsesMessageTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs))

    sck = await cg.gpio_pin_expression(config[CONF_SCK_PIN])
    cg.add(var.set_sck_pin(sck))

    mosi = await cg.gpio_pin_expression(config[CONF_MOSI_PIN])
    cg.add(var.set_mosi_pin(mosi))

    miso = await cg.gpio_pin_expression(config[CONF_MISO_PIN])
    cg.add(var.set_miso_pin(miso))

    gdo0 = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
    cg.add(var.set_gdo0_pin(gdo0))

    if CONF_GDO2_PIN in config:
        gdo2 = await cg.gpio_pin_expression(config[CONF_GDO2_PIN])
        cg.add(var.set_gdo2_pin(gdo2))

    cg.add(var.set_uart_num(config[CONF_UART_NUM]))
    cg.add(var.set_port(config[CONF_PORT]))

    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)


# Action: ramses_esp.send_hgi80
RAMSES_SEND_HGI80_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RamsesESPComponent),
        cv.Required(CONF_COMMAND): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "ramses_esp.send_hgi80", SendHgi80Action, RAMSES_SEND_HGI80_SCHEMA
)
async def ramses_send_hgi80_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_COMMAND], args, cg.std_string)
    cg.add(var.set_command(template_))
    return var
