import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_CARBON_MONOXIDE,
    CONF_FORMALDEHYDE,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_TVOC,
    CONF_TYPE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

DEPENDENCIES = ["uart"]

prosense_ns = cg.esphome_ns.namespace("prosense")
ProsenseComponent = prosense_ns.class_(
    "ProsenseComponent", cg.PollingComponent, uart.UARTDevice
)

TYPE_DSRF = "DS-RF"
TYPE_CO_100 = "CO-100"

ProsenseType = prosense_ns.enum("ProsenseType")

PROSENSE_TYPES = {
    TYPE_DSRF: ProsenseType.PROSENSE_TYPE_DSRF,
    TYPE_CO_100: ProsenseType.PROSENSE_TYPE_CO100,
}

SENSORS_TO_TYPE = {
    CONF_CARBON_MONOXIDE: [TYPE_CO_100],
    CONF_TEMPERATURE: [TYPE_DSRF],
    CONF_HUMIDITY: [TYPE_DSRF],
    CONF_TVOC: [TYPE_DSRF],
    CONF_FORMALDEHYDE: [TYPE_DSRF],
}


def validate_prosense_sensors(value):
    for key, types in SENSORS_TO_TYPE.items():
        if key in value and value[CONF_TYPE] not in types:
            raise cv.Invalid(f"{value[CONF_TYPE]} does not have {key} sensor!")
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ProsenseComponent),
            cv.Required(CONF_TYPE): cv.enum(PROSENSE_TYPES, upper=True),
            cv.Optional(CONF_CARBON_MONOXIDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_MONOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


def final_validate(config):
    schema = uart.final_validate_device_schema(
        "prosense", baud_rate=9600, require_rx=True, require_tx=True
    )
    schema(config)


FINAL_VALIDATE_SCHEMA = final_validate

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # cg.add(var.set_type(config[CONF_TYPE]))

    if CONF_CARBON_MONOXIDE in config:
        sens = await sensor.new_sensor(config[CONF_CARBON_MONOXIDE])
        cg.add(var.set_co_sensor(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))

    if CONF_TVOC in config:
        sens = await sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_tvoc_sensor(sens))

    if CONF_FORMALDEHYDE in config:
        sens = await sensor.new_sensor(config[CONF_FORMALDEHYDE])
        cg.add(var.set_formaldehyde_sensor(sens))
