#pragma once

// Project includes
#include "ActiveSensor.hpp"

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
	 *	Private Functions
	 */
	void calculateRpm();

	/*
	 *	Private Variables
	 */
	int64_t lastFallingEdgeTime_ = 0;
	int64_t fallingEdgeTime_ = 0;

	double hz_ = 0.0;

	uint16_t rpm_ = 0;
};
