#pragma once

// Project includes
#include "ActiveSensor.hpp"

// espidf includes
#include "freertos/FreeRTOS.h"

class Speed : public ActiveSensor
{
public:
	Speed();

	int get() override;

	/*
	 *	Public Callback functions
	 */
	IRAM_ATTR void cb() override;

private:
	/*
	 *	Private Variables
	 */
	volatile int64_t lastFallingEdgeTime_ = 0;
	volatile int64_t fallingEdgeTime_ = 0;
	portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

	double hz_ = 0.0;

	uint8_t kmh_ = 0;
};
