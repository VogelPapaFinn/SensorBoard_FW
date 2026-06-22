#pragma once

// Project includes
#include "PassiveSensor.hpp"

// C++ includes
#include <vector>

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
	std::vector<uint8_t> lastLevels_;

	double resistance_ = 0.0;

	float smoothedValue_ = -1.0f;
};
