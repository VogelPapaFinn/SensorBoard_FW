#pragma once

// espidf includes
#include "driver/gpio.h"
#include "esp_attr.h"

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
	IRAM_ATTR virtual void cb();

protected:
	/*
	 *	Private Variables
	 */
	bool enabled_ = false;

	gpio_num_t gpio_ = GPIO_NUM_NC;
};
