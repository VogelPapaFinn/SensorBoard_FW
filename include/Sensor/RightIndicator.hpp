#pragma once

// Project includes
#include "ActiveSensor.hpp"

class RightIndicator : public ActiveSensor
{
public:
	RightIndicator();

	int get() override;

	/*
	 *	Public Callback functions
	 */
	void isr() override;

private:
	/*
	 *	Private Variables
	 */
	bool high_ = false;

	gpio_num_t gpio_ = GPIO_NUM_15;
};
