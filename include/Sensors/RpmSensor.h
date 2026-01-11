#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*
 *	Public functions
 */
bool sensorsInitRpmSensor();

void sensorsActivateRpmISR();

void sensorsDeactivateRpmISR();

uint16_t sensorsGetRpm();