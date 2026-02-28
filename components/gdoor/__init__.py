import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID
from esphome.components.esp32 import include_builtin_idf_component

CODEOWNERS = ["@dtill"]
DOMAIN = "gdoor"
DEPENDENCIES = []
MULTI_CONF = True

gdoor_esphome_ns = cg.esphome_ns.namespace("gdoor_esphome")

GdoorComponent = gdoor_esphome_ns.class_("GdoorComponent", cg.Component)

CONF_TX_PIN = "tx_pin"
CONF_TX_EN_PIN = "tx_en_pin"
CONF_RX_PIN = "rx_pin"
CONF_RX_THRESH_PIN = "rx_thresh_pin"
CONF_RX_SENS = "rx_sens"

DEFAULT_TX_PIN = 25
DEFAULT_TX_EN_PIN = 27
DEFAULT_RX_PIN = 22
DEFAULT_RX_THRESH_PIN = 26


RX_SENS_MODES = {
    "low": 1.3,
    "med": 1.45,
    "high": 1.65,
}
DEFAULT_RX_SENS_MODE = "high"


def validate_rx_sens_and_pin(cfg):
    """
    Enforces:
      - If the raw rx_pin is not 22, then if rx_sens is provided, it must be "high".
    """
    rx_pin_cfg = cfg.get(CONF_RX_PIN, DEFAULT_RX_PIN)
    if isinstance(rx_pin_cfg, dict):
        rx_pin_number = rx_pin_cfg.get("number", DEFAULT_RX_PIN)
    else:
        rx_pin_number = rx_pin_cfg
    # Now, apply the validation: when rx_pin is not exactly 22, the rx_sens (if given) must be "high".
    if rx_pin_number != 22:
        # Only check rx_sens if it's explicitly provided.
        if CONF_RX_SENS in cfg and cfg[CONF_RX_SENS] != "high":
            raise cv.Invalid("If rx_pin is not 22, rx_sens must be 'high'.")
    return cfg

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(GdoorComponent),
        cv.Optional(CONF_TX_PIN, default=DEFAULT_TX_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_TX_EN_PIN, default=DEFAULT_TX_EN_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RX_PIN, default=DEFAULT_RX_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_RX_THRESH_PIN, default=DEFAULT_RX_THRESH_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_RX_SENS, default=DEFAULT_RX_SENS_MODE): cv.enum(RX_SENS_MODES, upper=False),
    }).extend(cv.COMPONENT_SCHEMA),
    validate_rx_sens_and_pin
)

async def to_code(config):
    include_builtin_idf_component("esp_driver_dac")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
    cg.add(var.set_tx_pin(tx_pin))
    tx_en_pin = await cg.gpio_pin_expression(config[CONF_TX_EN_PIN])
    cg.add(var.set_tx_en_pin(tx_en_pin))
    rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
    cg.add(var.set_rx_pin(rx_pin))
    rx_thresh_pin = await cg.gpio_pin_expression(config[CONF_RX_THRESH_PIN])
    cg.add(var.set_rx_thresh_pin(rx_thresh_pin))
    if CONF_RX_SENS in config:
        cg.add(var.set_rx_sens(config[CONF_RX_SENS]))
