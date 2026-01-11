#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*
 *	Public functions
 */
bool sensorsInitSpeedSensor();

void sensorsActivateSpeedISR();

void sensorsDeactivateSpeedISR();

uint8_t sensorsGetSpeed();