#pragma once

// Project include
#include "State/State.hpp"
#include "SystemContext.hpp"

class Registration : public State
{
public:
	Registration(SystemContext* p_sysCon);

	~Registration();

	void enter() override;

private:
	/*
	 *	Private Functions
	 */
	void registerToEvents();

	void nextDisplay();

	void wakeUpAllDisplays() const;

	/*
	 *	Private Variables
	 */
	SystemContext* sysCon_ = nullptr;

	uint8_t currDisplay = 0;
};
