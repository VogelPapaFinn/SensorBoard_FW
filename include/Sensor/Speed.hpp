#pragma once

// Project includes
#include "ActiveSensor.hpp"

class Speed : public ActiveSensor
{
public:
	Speed();

	int get() override;

	/*
	 *	Public Callback functions
	 */
	void cb() override;

private:
	/*
	 *	Private Functions
	 */
	void calculateKmh();

	/*
	 *	Private Variables
	 */
	int64_t lastFallingEdgeTime_ = 0;
	int64_t fallingEdgeTime_ = 0;

	double hz_ = 0.0;

	uint8_t kmh_ = 0;
};
