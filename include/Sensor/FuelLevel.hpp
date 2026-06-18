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

	double lastResistance_ = 0.0;
	double resistance_ = 0.0;
};
