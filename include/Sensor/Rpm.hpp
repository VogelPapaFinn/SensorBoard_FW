#pragma once

// Project includes
#include "ActiveSensor.hpp"

// C++ includes
#include <list>

// espidf includes
#include "freertos/FreeRTOS.h"

class Rpm : public ActiveSensor
{
public:
	Rpm();

	int get() override;

	/*
	 *	Public Callback functions
	 */
	void cb() override;

private:
	/*
	 *	Private Variables
	 */
	int64_t lastFallingEdgeTime_ = 0;
	int64_t fallingEdgeTime_ = 0;
	portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

	float hz_ = 0.0;

	std::list<uint16_t> lastRpm_ = {0, 0, 0};
};
