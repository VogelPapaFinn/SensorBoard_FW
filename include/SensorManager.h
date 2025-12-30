#pragma once

// C includes
#include <stdbool.h>

/*
 * Defines
 */

// Defines used for the voltage divider/ohmmeter to measure
// the oil pressure, fuel level and water temperature
#define OIL_FUEL_WATER_VOLTAGE_V 3.3
#define OIL_FUEL_R1 240
#define WATER_R1 3000

// GPIOs
#define GPIO_OIL_PRESSURE GPIO_NUM_12
#define GPIO_FUEL_LEVEL GPIO_NUM_11
#define GPIO_WATER_TEMPERATURE GPIO_NUM_13
#define GPIO_SPEED GPIO_NUM_14
#define GPIO_RPM GPIO_NUM_21

// ADC CHANNELS
#define ADC_CHANNEL_OIL_PRESSURE ADC_CHANNEL_1
#define ADC_CHANNEL_FUEL_LEVEL ADC_CHANNEL_0
#define ADC_CHANNEL_WATER_TEMPERATURE ADC_CHANNEL_2
#define ADC_CHANNEL_INT_TEMPERATURE ADC_CHANNEL_6

// OIL PRESSURE THRESHOLDS
#define OIL_LOWER_VOLTAGE_THRESHOLD 65 // mV -> R2 ~= 5 Ohms
#define OIL_UPPER_VOLTAGE_THRESHOLD 255// mV -> R2 ~= 20 Ohms

// FUEL LEVEL CALCULATION STUFF
#define FUEL_LEVEL_OFFSET 5.0f
#define FUEL_LEVEL_TO_PERCENTAGE 115.0f// Divide the calculated resistance by this value to get the level in percent

/*
 *	Public Stuff
 */
typedef enum
{
	SENSOR_OIL_PRESSURE,
	SENSOR_FUEL_LEVEL_PERCENT,
	SENSOR_FUEL_LEVEL_LITRE,
	SENSOR_WATER_TEMPERATURE,
	SENSOR_INTERNAL_TEMPERATURE,
	SENSOR_SPEED,
	SENSOR_RPM,
	SENSOR_LEFT_INDICATOR,
	SENSOR_RIGHT_INDICATOR,
} SENSOR_T;

/*
 *	Functions
 */
//! \brief Initializes the SensorManager
//! \retval false - Initialization failed
//! \retval true - Initialization succeeded without errors
bool sensorManagerInit(void);

//! \brief Checks if there is oil pressure
bool sensorManagerUpdateOilPressure(void);

//! \brief Updates the fuel level
bool sensorManagerUpdateFuelLevel(void);

//! \brief Updates the water temperature
bool sensorManagerUpdateWaterTemperature(void);

//! \brief Enables the speed ISR
//! \retval Boolean indicating if it worked
bool sensorManagerEnableSpeedISR(void);

//! \brief Disables the speed ISR
void sensorManagerDisableSpeedISR(void);

//! \brief Updates the speed
bool sensorManagerUpdateSpeed(void);

//! \brief Enables the rpm ISR
//! \retval Boolean indicating if it worked
bool sensorManagerEnableRpmISR(void);

//! \brief Disables the rpm ISR
void sensorManagerDisableRpmISR(void);

//! \brief Updates the rpm
bool sensorManagerUpdateRPM(void);

//! \brief Updates the internal temperature
bool sensorManagerUpdateInternalTemperature(void);
