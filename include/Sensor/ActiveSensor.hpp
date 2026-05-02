#pragma once

// espidf includes
#include "driver/gpio.h"

class ActiveSensor
{
public:
	ActiveSensor(const gpio_int_type_t& triggeringEdge);

	void enable();

	void disable();

	virtual int get();

	/*
	 *	Public Callback functions
	 */
	virtual void isr();

protected:
	/*
	 *	Private Variables
	 */
	bool enabled_ = false;

	gpio_num_t gpio_ = GPIO_NUM_NC;
};
