#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public typedefs
 */
//! \brief Typedef enum indicating which manual (to measure) sensor is meant
typedef enum
{
	OIL_PRESSURE,
	FUEL_LEVEL,
	WATER_TEMP,
	INTERNAL_TEMP
} ManualReadSensor_t;

//! \brief Typedef enum indicating which automatic (to measure) sensor is meant
typedef enum
{
	SPEED,
	RPM,
	L_INDICATOR,
	R_INDICATOR,
} AutomaticReadSensor_t;

/*
 *	Public functions
 */
//! \brief Initializes the Sensor Manager by initializing the ADC's, ISR services etc.
void sensorManagerInit();

//! \brief Starts periodically reading all available sensors
//! \retval Boolean indicating if everything went successful
bool sensorManagerStartReadingAllSensors();

//! \brief Stops reading all available sensors
//! \retval Boolean indicating if everything went successful
bool sensorManagerStopReadingAllSensors();

//! \brief Starts periodically sending all available sensor data via CAN bus
//! \retval Boolean indicating if everything went successful
bool sensorManagerStartSendingSensorData();

//! \brief Stops periodically sending all available sensor data via CAN bus
void sensorManagerStopSendingSensorData();

//! \brief Starts periodically reading the specified manual sensor
//! \param sensor A ManualReadSensor_t instance, indicating which sensor should be read
//! periodically
//! \retval Boolean indicating if everything went successful
bool sensorManagerStartReadingManualSensor(ManualReadSensor_t sensor);

//! \brief Starts periodically reading the specified manual sensor
//! \param sensor A ManualReadSensor_t instance, indicating which sensor should no
//! longer be read periodically
void sensorManagerStopReadingManualSensor(ManualReadSensor_t sensor);

//! \brief Starts reading the specified automatic sensor
//! \param sensor A AutomaticReadSensor_t instance, indicating which sensor should be read
//! \retval Boolean indicating if everything went successful
bool sensorManagerStartReadingAutomaticSensor(AutomaticReadSensor_t sensor);

//! \brief Stops reading the specified automatic sensor
//! \param sensor A AutomaticReadSensor_t instance, indicating which sensor should no
//! longer be read
void sensorManagerStopReadingAutomaticSensor(AutomaticReadSensor_t sensor);