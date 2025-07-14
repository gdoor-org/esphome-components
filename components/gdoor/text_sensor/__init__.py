import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_NAME
from .. import DOMAIN, GdoorComponent, gdoor_esphome_ns

CODEOWNERS = ["@dtill"]
DEPENDENCIES = [DOMAIN]

# Define the text sensor class for gdoor
GDoorBusMessage = gdoor_esphome_ns.class_("GDoorBusMessage", text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(GDoorBusMessage).extend({
    cv.Required(CONF_NAME): cv.string,
    cv.Required("gdoor_id"): cv.use_id(GdoorComponent),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    parent = await cg.get_variable(config["gdoor_id"])
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    cg.add(var.set_parent(parent))
