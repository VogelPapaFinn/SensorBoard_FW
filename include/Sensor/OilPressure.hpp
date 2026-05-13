#pragma once

// Project includes
#include "PassiveSensor.hpp"

class OilPressure : public PassiveSensor
{
public:
	OilPressure(adc_oneshot_unit_handle_t* adc);

	int get();

protected:
	/*
	 *	Private Functions
	 */
	void specificRead() override;

	/*
	 *	Private Variables
	 */
	bool pressure_ = false;
};
