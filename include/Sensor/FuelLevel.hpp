#pragma once

// Project includes
#include "PassiveSensor.hpp"

class FuelLevel : public PassiveSensor
{
public:
	FuelLevel(adc_oneshot_unit_handle_t* adc);

	int get();

protected:
	/*
	 *	Private Functions
	 */
	void specificRead() override;

	void calcLevel();

	/*
	 *	Private Variables
	 */
	int levelInPercent_ = 0;

	double resistance_ = 0.0;

	adc_channel_t channel_ = ADC_CHANNEL_0;

	gpio_num_t gpio_ = GPIO_NUM_1;
};
