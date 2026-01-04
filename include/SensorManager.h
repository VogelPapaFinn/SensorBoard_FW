#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initializes the Sensor Manager by initializing the ADC's, ISR services etc.
void sensorManagerInit();

//! \brief Starts periodically reading all available sensors
//! \retval Boolean indicating if everything went successful
bool sensorsStartReadingAllSensors();

//! \brief Starts periodically sending all available sensor data via CAN bus
//! \retval Boolean indicating if everything went successful
bool sensorsStartSendingSensorData();

//! \brief Stops periodically sending all available sensor data via CAN bus
void sensorsStopSendingSensorData();