#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initializes the Sensor Manager by initializing the ADC's, ISR services etc.
void sensorManagerInit();

void sensorsActivateISRs();

void sensorsDeactivateISRs();

//! \brief Starts periodically reading all available sensors
//! \retval Boolean indicating if everything went successful
void sensorsReadAll();

//! \brief Starts periodically sending all available sensor data via CAN bus
//! \retval Boolean indicating if everything went successful
void sensorsSendAll();