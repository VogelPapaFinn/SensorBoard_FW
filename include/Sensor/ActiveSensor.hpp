#pragma once

// espidf includes
#include "driver/gpio.h"

class ActiveSensor
{
public:
	ActiveSensor(gpio_num_t gpio, const gpio_int_type_t& triggeringEdge);

	void enable();

	void disable();

	virtual int get();

	/*
	 *	Public Callback functions
	 */
	virtual void cb();

protected:
	/*
	 *	Private Variables
	 */
	bool enabled_ = false;

	gpio_num_t gpio_ = GPIO_NUM_NC;
};
