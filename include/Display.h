#pragma once

// Project includes
#include "can.h"

// C includes
#include <stdbool.h>
#include <stdint.h>

/*
 *	Public defines
 */
#define UUID_LENGTH_B 7
#define FIRMWARE_LENGTH_B 13
#define COMMIT_LENGTH_B 9

/*
 *	Public typedefs
 */
//! \brief A struct representing an uuid which got a comId assigned
typedef struct
{
	//! \brief The uuid of the display
	uint8_t uuid[UUID_LENGTH_B];

	//! \brief The assigned comId. 0 if this entry is faulty/unused
	uint8_t comId;

	//! \brief The screen the display shows
	Screen_t screen;

	//! \brief The firmware version string of the display
	char firmwareVersion[FIRMWARE_LENGTH_B];

	//! \brief The commit hash string of the firmware version
	char commitHash[COMMIT_LENGTH_B];
} DisplayConfig_t;

/*
 *	Public functions
 */
//! \brief Registers a new display and keeps track of it if needed
//! \param p_uuid The uuid of the display
//! \retval Bool indicating if  the display is being tracked or not
DisplayConfig_t* displayRegister(const uint8_t* p_uuid);

//! \brief Updates the firmware version of a display
//! \param comId The com id of the display
//! \param p_firmware The new firmware version
void displaySetFirmwareVersion(uint8_t comId, const uint8_t* p_firmware);

//! \brief Updates the commit information of a display
//! \param comId The com id of the display
//! \param p_commit The new commit information
void displaySetCommitInformation(uint8_t comId, const uint8_t* p_commit);

//! \brief Restarts the display with the given com id
//! \param comId The com id of the display
void displayRestart(uint8_t comId);

//! \brief Checks if all displays registered themselves
//! \retval Bool indicating result
bool displayAllRegistered();