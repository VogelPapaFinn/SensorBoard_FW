#pragma once

// Project includes
#include "SystemContext.hpp"
#include "Can.hpp"

class RegistrationHandler
{
public:
	RegistrationHandler(SystemContext* p_sysCon);

private:
	/*
	 *	Private Functions
	 */
	void handleRegistration(const Can::Frame* p_frame) const;

	void wakeUpAllDisplays() const;

	/*
	 *	Private Variables
	 */
	SystemContext* sysCon_ = nullptr;
};