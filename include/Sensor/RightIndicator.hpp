#pragma once

// Project includes
#include "ActiveSensor.hpp"

class RightIndicator : public ActiveSensor
{
public:
	RightIndicator();

	int get() override;

	/*
	 *	Public Callback functions
	 */
	void cb() override;

private:
	/*
	 *	Private Functions
	 */
	void notifyAboutNewValue() override;

	/*
	 *	Private Variables
	 */
	bool active_ = false;
};
