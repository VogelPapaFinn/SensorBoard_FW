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
	void isr() override;

private:
	/*
	 *	Private Functions
	 */
	void calculateRpm();

	/*
	 *	Private Variables
	 */
	gpio_num_t gpio_ = GPIO_NUM_9;

	int64_t lastFallingEdgeTime_ = 0;
	int64_t fallingEdgeTime_ = 0;

	double hz_ = 0.0;

	uint16_t rpm_ = 0;
};
