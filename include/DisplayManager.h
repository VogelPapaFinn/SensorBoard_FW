#pragma once

// C includes
#include <stdint.h>

/*
 *	Public functions
 */
//! \brief Initiates the Display Manager by loading the needed configuration file
void displayManagerInit();

//! \brief Restarts the display with the given com id
//! \param comId The com id of the display
void displayRestart(uint8_t comId);

//! \brief Starts broadcasting the request for the displays to register themselves
void displayStartRegistrationProcess();

//! \brief Registers a display with the given uuid
//! \param p_uuid The uuid array of the display. Usually 6 Bytes long
void displayRegisterWithUUID(const uint8_t* p_uuid);

//! \brief Debug function to print the content of the configuration file
void displayPrintConfigFile();