#pragma once

// C includes
#include <stdint.h>
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initiates the Display Center by loading the needed configuration file
void displayCenterInit();

//! \brief Restarts the display with the given com id
//! \param comId The com id of the display
void displayRestart(uint8_t comId);

//! \brief Starts broadcasting the request for the displays to register themselves
void displayStartRegistrationProcess();

//! \brief Registers a display with the given uuid
//! \param p_uuid The uuid array of the display. Usually 6 Bytes long
//! \retval Returns the new comid or 0 if an error occured
void displayRegisterWithUUID(const uint8_t* p_uuid, uint8_t* p_senderId, uint8_t* p_screen);

void displaySetFirmwareVersion(uint8_t comId, const uint8_t* p_firmware);

void displaySetCommitInformation(uint8_t comId, const uint8_t* p_information);

bool displayAllRegistered();