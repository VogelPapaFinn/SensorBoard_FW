#pragma once

// C includes
#include <stdbool.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*
 *	Public functions
 */
bool sensorRpmInit();

void sensorRpmActivateISR();

void sensorRpmDeactivateISR();

uint16_t sensorRpmGet();