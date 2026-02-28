import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_NAME
from .. import DOMAIN, GdoorComponent, gdoor_esphome_ns, GDOOR_BUSDATA_VALIDATOR

CODEOWNERS = ["@dtill"]
DEPENDENCIES = [DOMAIN]

# Define the text sensor class for gdoor
GDoorActionSensor = gdoor_esphome_ns.class_("GDoorActionSensor", binary_sensor.BinarySensor, cg.Component)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(GDoorActionSensor).extend({
    cv.Required(CONF_NAME): cv.string,
    cv.Required("gdoor_id"): cv.use_id(GdoorComponent),
    cv.Optional("busdata", default=[]): cv.ensure_list(GDOOR_BUSDATA_VALIDATOR),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    parent = await cg.get_variable(config["gdoor_id"])
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(var.set_parent(parent))
    for busdata in config["busdata"]:
        cg.add(var.add_busdata(busdata))
