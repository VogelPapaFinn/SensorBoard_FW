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
	void isr() override;

private:
	/*
	 *	Private Functions
	 */
	void calculateKmh();

	/*
	 *	Private Variables
	 */
	gpio_num_t gpio_ = GPIO_NUM_10;

	int64_t lastFallingEdgeTime_ = 0;
	int64_t fallingEdgeTime_ = 0;

	double hz_ = 0.0;

	uint8_t kmh_ = 0;
};
