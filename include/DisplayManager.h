#pragma once

// C includes
#include <stdint.h>

/*
 *	Public functions
 */
void displayManagerInit();

void displayRestart(uint8_t comId);

void displayStartRegistrationProcess();

void displayRegisterWithUUID(const uint8_t* p_uuid);

void displayPrintConfigFile();