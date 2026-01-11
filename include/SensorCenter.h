#pragma once

/*
 *	Public functions
 */
//! \brief Initializes the Sensor Center by initializing the ADC's, ISR services etc.
void sensorCenterInit();

//! \brief Activate the ISRs needed for speed and rpm
void sensorsActivateISRs();

//! \brief Deactivate the ISRs needed for speed and rpm
void sensorsDeactivateISRs();

//! \brief Reads all available sensors once
void sensorsReadAll();

//! \brief Sends all available sensor data via CAN bus once
void sensorsSendAll();