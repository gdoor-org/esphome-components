import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import event
from esphome.const import CONF_ID
from .. import DOMAIN, GdoorComponent, gdoor_esphome_ns, GDOOR_BUSDATA_VALIDATOR

CODEOWNERS = ["@dtill"]
DEPENDENCIES = [DOMAIN]

GDoorBusEvent = gdoor_esphome_ns.class_("GDoorBusEvent", event.Event, cg.Component)


def validate_event_config(config):
    """
    Cross-field validation:
    - If busdata provided: auto-derive event_types from its keys.
    - If event_types explicitly provided AND busdata also provided:
      every busdata key must appear in event_types.
    - If neither provided: error.
    """
    busdata = config.get("busdata", {})
    explicit_types = config.get("event_types", [])

    if not busdata and not explicit_types:
        raise cv.Invalid(
            "Provide 'busdata' with at least one entry, or 'event_types' "
            "(for TX-only events linked via tx_event_id on an output)"
        )
    if busdata and explicit_types:
        for key in busdata:
            if key not in explicit_types:
                raise cv.Invalid(
                    f"busdata key '{key}' is not listed in event_types. "
                    f"Add it, or remove event_types to auto-derive from busdata keys."
                )
    return config


CONFIG_SCHEMA = cv.All(
    event.event_schema(GDoorBusEvent).extend({
        cv.Required("gdoor_id"): cv.use_id(GdoorComponent),
        cv.Optional("event_types"): cv.ensure_list(cv.string_strict),
        cv.Optional("busdata", default={}): cv.Schema({
            # event_type_name → list of validated hex frame strings
            cv.string_strict: cv.ensure_list(GDOOR_BUSDATA_VALIDATOR),
        }),
    }).extend(cv.COMPONENT_SCHEMA),
    validate_event_config,
)


async def to_code(config):
    parent = await cg.get_variable(config["gdoor_id"])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Derive event_types: explicit list takes priority; otherwise auto from busdata keys
    event_types = config.get("event_types") or list(config["busdata"].keys())

    # register_event handles: App.register_event, set_event_types, device_class,
    # on_event automations, MQTT, web_server — pass event_types as required kwarg
    await event.register_event(var, config, event_types=event_types)

    cg.add(var.set_parent(parent))
    cg.add(parent.register_bus_listener(var))

    # Register each busdata hex string → event_type mapping
    for event_type_name, payloads in config["busdata"].items():
        for payload in payloads:
            cg.add(var.add_busdata(payload, event_type_name))
