#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initializes the operation manager
//! \retval Bool indicating if the initialization was successful
bool operationManagerInit();

void operationManagerStartReadingSensors();

void operationManagerStopReadingSensors();

void operationManagerStartSendingSensors();

void operationManagerStopRSendingSensors();

//! \brief Destroys the operation manager
void operationManagerDestroy();