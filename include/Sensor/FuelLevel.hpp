#pragma once

// Project includes
#include "PassiveSensor.hpp"

// C++ includes
#include <list>

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
	std::list<uint8_t> lastLevels_ = std::list<uint8_t>(21, 0);

	double resistance_ = 0.0;

	float smoothedValue_ = -1.0f;
};
