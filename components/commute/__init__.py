"""Commute: the ESPHome side. register_plugin() reads the id, version, tile memory and the screen's texts from the
manifest and translations/ beside this folder; the four text sensors are what the top bar's item reads."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]
AUTO_LOAD = ["text_sensor"]

commute_ns = cg.esphome_ns.namespace("commute")
Commute = commute_ns.class_("Commute", cg.Component)

CONF_MODE = "mode"
CONF_PHASE = "phase"
CONF_PROGRESS = "progress"
CONF_PERSON = "person"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Commute),
        cv.Optional(CONF_MODE): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_PHASE): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_PROGRESS): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_PERSON): cv.use_id(text_sensor.TextSensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    for key, setter in ((CONF_MODE, "set_mode"), (CONF_PHASE, "set_phase"), (CONF_PROGRESS, "set_progress"),
                        (CONF_PERSON, "set_person")):
        if key in config:
            cg.add(getattr(var, setter)(await cg.get_variable(config[key])))
