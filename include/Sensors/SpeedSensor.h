#pragma once

// C includes
#include <stdbool.h>
#include <stdint.h>

/*
 *	Public functions
 */
bool sensorSpeedInit();

void sensorSpeedActivateISR();

void sensorSpeedDeactivateISR();

uint8_t sensorSpeedGet();