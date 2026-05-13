#pragma once

// Project includes
#include "EcuPids.h"

// C++ includes
#include <functional>
#include <string>
#include <unordered_map>

/*
 *	constexpr
 */
constexpr auto UNIT_PERCENT = "%";
constexpr auto UNIT_VOLT = "V";
constexpr auto UNIT_CELSIUS = "°C";
constexpr auto UNIT_MS = "ms";
constexpr auto UNIT_DEGREE = "°";

constexpr auto BITMASK_FUNCTION = [](const uint32_t raw) { return raw > 0 ? 1.0 : 0.0; };

/*
 *	Public Struct
 */
struct EcuSensor
{
	std::string name;

	uint8_t responseByteCount = 1;
	std::string unit = "";
	uint8_t bitInMask = 0;

	uint16_t rawValue = 0;
	std::function<double(uint16_t)> convertRawValue;

	double getConvertedValue() const
	{
		if (convertRawValue) {
			return convertRawValue(rawValue);
		}

		return 0.0;
	}
};

/*
 *	Public ECU Sensor Map
 */
// TODO: Find out the bits of the different bitmask entries
static std::unordered_map<uint16_t, EcuSensor> ECU_SENSORS{
	{ALTERNATOR_LOAD_P,
	 EcuSensor{.name = "Alternator - Load", .responseByteCount = 1, .unit = UNIT_PERCENT,
	           .convertRawValue = [](const uint16_t raw) { return raw / 2.55; }}},

	{ALTERNATOR_DESIRED_VOLTAGE,
	 EcuSensor{.name = "Alternator - Desired Voltage", .responseByteCount = 1, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return raw / 10; }}},

	{CABIN_FAN_ON,
	 EcuSensor{.name = "Cabin Fan - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{BRAKE_ON,
	 EcuSensor{.name = "Brake - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{REAR_DEFROSTER_ON,
	 EcuSensor{.name = "Rear Window Defroster - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{DAYTIME_LIGHTS_ON,
	 EcuSensor{.name = "Daytime Lights - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{COOLANT_V,
	 EcuSensor{.name = "Coolant - Sensor Voltage", .responseByteCount = 2, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return raw * (5 / 1023); }}},

	{COOLANT_C,
	 EcuSensor{.name = "Alternator - Temperature", .responseByteCount = 1, .unit = UNIT_CELSIUS,
	           .convertRawValue = [](const uint16_t raw) { return raw - 40; }}},

	{EGR_ON,
	 EcuSensor{.name = "EGR - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{FAN_SPEED_SLOW,
	 EcuSensor{.name = "Cooling Fan Speed - On, Slow Speed", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{FAN_SPEED_MEDIUM,
	 EcuSensor{.name = "Cooling Fan Speed - On, Medium Speed", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{FAN_SPEED_FAST,
	 EcuSensor{.name = "Cooling Fan Speed - On, Fast Speed", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{FUEL_PUMP_ON,
	 EcuSensor{.name = "Fuel Pump - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	// TODO: Find out the scale of the injection time
	{INJECTION_MS,
	 EcuSensor{.name = "Injector - Injection Time", .responseByteCount = 2, .unit = UNIT_MS,
	           .convertRawValue = [](const uint16_t raw) { return raw / 1000.0; }}},

	{LOW_VOLTAGE_LIGHT_ON,
	 EcuSensor{.name = "Battery - Low Voltage (Light)", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{HEADLIGHT_LEFT_ON,
	 EcuSensor{.name = "Headlight Left - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{HEADLIGHT_RIGHT_ON,
	 EcuSensor{.name = "Headlight Right - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{IDLE_BYPASS_MS,
	 EcuSensor{.name = "Idle Bypass - Time", .responseByteCount = 2, .unit = UNIT_MS,
	           .convertRawValue = [](const uint16_t raw) { return raw * 0.002; }}},

	{IDLE_SWITCH_ON,
	 EcuSensor{.name = "Idle Switch - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{ENGINE_LOAD_P,
	 EcuSensor{.name = "Engine - Load", .responseByteCount = 2, .unit = UNIT_PERCENT,
	           .convertRawValue = [](const uint16_t raw) { return raw / 40.96; }}},

	{AIR_MASS_V,
	 EcuSensor{.name = "Airmass - Voltage", .responseByteCount = 2, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return raw * (5 / 1023); }}},

	// TODO: Test if "calculation" is correct
	{AIR_MASS_GS,
	 EcuSensor{.name = "Airmass - g/s", .responseByteCount = 2, .unit = "g/s",
	           .convertRawValue = [](const uint16_t raw) { return raw; }}},

	{MAIN_RELAY_ON,
	 EcuSensor{.name = "Main Relais - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{CHECK_ENGINE_LIGHT,
	 EcuSensor{.name = "Engine - Check Engine Light", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{PRE_CAT_LAMBDA_V,
	 EcuSensor{.name = "Airmass Pre-Cat - Voltage", .responseByteCount = 2, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return raw * (5 / 1023); }}},

	{PRESSURE_SWITCH_POWER_STEERING_ON,
	 EcuSensor{.name = "Power Steering - Pressure Switch On", .responseByteCount = 1,
	           .convertRawValue = [](const uint16_t raw) { return raw > 0 ? 1.0 : 0.0; }}},

	{RPM,
	 EcuSensor{.name = "RPM - Value", .responseByteCount = 2,
	           .convertRawValue = [](const uint16_t raw) { return raw; }}},

	{EGR_VALVE_POSITION,
	 EcuSensor{.name = "EGR - Valve Position", .responseByteCount = 1,
	           .convertRawValue = [](const uint16_t raw) { return raw; }}},

	{ADVANCING_IGNITION_DEG,
	 EcuSensor{.name = "Ignition Timing - Advancing", .responseByteCount = 2, .unit = UNIT_DEGREE,
	           .convertRawValue = [](const uint16_t raw) { return (raw - 255) / 12.8; }}},

	{IMMOBILIZER_ON,
	 EcuSensor{.name = "Immobilizer - On", .responseByteCount = 1, .bitInMask = 0b00000001,
	           .convertRawValue = BITMASK_FUNCTION}},

	{THROTTLE_POSITION_V,
	 EcuSensor{.name = "Throttle - Voltage", .responseByteCount = 2, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return raw * (5 / 1023); }}},

	{ECU_INPUT_VOLTAGE,
	 EcuSensor{.name = "ECU - Input Voltage", .responseByteCount = 1, .unit = UNIT_VOLT,
	           .convertRawValue = [](const uint16_t raw) { return (raw / 12.8) + 1; }}},

	{SPEED_KMH,
	 EcuSensor{.name = "Speed - KMH", .responseByteCount = 1, .unit = "KMH",
	           .convertRawValue = [](const uint16_t raw) { return raw; }}},
};
