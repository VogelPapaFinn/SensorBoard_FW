#pragma once

// Project includes
#include "PassiveSensor.hpp"

class WaterTemperature : public PassiveSensor
{
public:
	WaterTemperature(adc_oneshot_unit_handle_t* adc);

	int get();

protected:
	/*
	 *	Private Functions
	 */
	void specificRead() override;

	void calcTemperature(uint16_t r);

	/*
	 *	Private Variables
	 */
	int temperature_ = 0;

	double resistance_ = 0.0;
};
