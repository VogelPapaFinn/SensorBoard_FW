#pragma once

// C includes
#include <stdbool.h>
#include <stdint.h>

/*
 *	Public functions
 */
void canUpdateManagerInit();

//! \brief Checks the updates folder on the SD Card for an update file
//! \retval Bool indicating if an update file was found
//! \note The first update file found is used. The version number does not get checked!
bool displayUpdateCanIsUpdateAvailable();

//! \brief Tries to update the specified display via CAN
//! \param comId the com id of the display
//! \retval Bool indicating if an update try was started
bool displayUpdateCanStart(uint8_t comId);