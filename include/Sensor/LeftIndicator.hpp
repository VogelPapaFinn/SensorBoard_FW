#pragma once

// Project includes
#include "ActiveSensor.hpp"

class LeftIndicator : public ActiveSensor
{
public:
	LeftIndicator();

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
