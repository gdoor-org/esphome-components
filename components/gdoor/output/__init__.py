import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_NAME
from .. import DOMAIN, GdoorComponent, gdoor_esphome_ns

CODEOWNERS = ["@dtill"]
DEPENDENCIES = [DOMAIN]

CONF_REQUIRE_RESPONSE = "require_response"
CONF_PAYLOAD = "payload"
CONF_TX_EVENT_ID = "tx_event_id"
CONF_TX_EVENT_TYPE = "tx_event_type"
HEX_STRING_REGEX = re.compile(r"^[0-9A-Fa-f]+$")  # Regex to validate hex string


GDoorBusWrite = gdoor_esphome_ns.class_("GDoorBusWrite", output.BinaryOutput, cg.Component)
# Reference only — avoids circular import; full class is defined in event/__init__.py
GDoorBusEvent = gdoor_esphome_ns.class_("GDoorBusEvent")

def calculate_crc(hex_string):
    """Calculate the CRC checksum of a given hex string."""
    if not HEX_STRING_REGEX.match(hex_string):
        raise cv.Invalid("Payload must be a valid hexadecimal string (e.g., '011041A1B14A0000A18F1E')")
    if len(hex_string) % 2 != 0:
        raise cv.Invalid("Hex string must contain an even number of digits (each byte consists of two hex digits).")
    checksum = 0
    for i in range(0, len(hex_string), 2):
        byte_hex = hex_string[i:i+2]
        byte_dec = int(byte_hex, 16)
        checksum = (checksum + byte_dec) % 256  # 8-bit overflow simulation
    return f"{checksum:02X}"

def validate_payload_with_crc(value):
    """Validate that the payload is a valid hex string and check its CRC."""
    if not HEX_STRING_REGEX.match(value):
        raise cv.Invalid("Payload must be a valid hexadecimal string (e.g., '011041A1B14A0000A18F1E')")
    if len(value) % 2 != 0:
        raise cv.Invalid("Hex string must contain an even number of digits (each byte consists of two hex digits).")
    provided_crc = value[-2:]               # Assume the last byte is the CRC
    data_without_crc = value[:-2]           # Data excluding CRC
    expected_crc = calculate_crc(data_without_crc)
    if provided_crc.upper() != expected_crc:
        raise cv.Invalid(f"CRC Checksum mismatch: provided {provided_crc.upper()}, expected {expected_crc} (Payload: {value})")
    return value

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(GDoorBusWrite),
    cv.Required(CONF_NAME): cv.string,
    cv.Required("gdoor_id"): cv.use_id(GdoorComponent),
    cv.Required(CONF_PAYLOAD): cv.All(
        cv.string,
        validate_payload_with_crc
    ),
    cv.Optional(CONF_REQUIRE_RESPONSE, default=False): cv.boolean,
    cv.Optional(CONF_TX_EVENT_ID): cv.use_id(GDoorBusEvent),
    cv.Optional(CONF_TX_EVENT_TYPE, default="press"): cv.string_strict,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    parent = await cg.get_variable(config["gdoor_id"])
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_parent(parent))
    cg.add(var.set_payload(config[CONF_PAYLOAD]))
    cg.add(var.set_require_response(config[CONF_REQUIRE_RESPONSE]))
    if CONF_TX_EVENT_ID in config:
        tx_event = await cg.get_variable(config[CONF_TX_EVENT_ID])
        cg.add(var.set_tx_event(tx_event))
        cg.add(var.set_tx_event_type(config[CONF_TX_EVENT_TYPE]))