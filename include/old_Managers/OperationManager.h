#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initializes the operation manager
//! \retval Bool indicating if the initialization was successful
bool operationManagerInit();

//! \brief Starts to read all sensors periodically
void operationManagerStartReadingSensors();

//! \brief Stops reading all sensors periodically
void operationManagerStopReadingSensors();

//! \brief Starts to send all sensor data periodically via CAN
void operationManagerStartSendingSensors();

//! \brief Stops sending all sensor data periodically via CAN
void operationManagerStopRSendingSensors();

//! \brief Destroys the operation manager
void operationManagerDestroy();