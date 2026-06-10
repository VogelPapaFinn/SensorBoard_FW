#pragma once

// Project includes
#include "Can.hpp"
#include "Core.hpp"

class State
{
public:
	/*
	 *	Public enum
	 */
	typedef enum
	{
		UNKNOWN,
		REGISTRATION,
		OPERATION,
		WIFI_OTA_UPDATE
	} TYPE;

	/*
	 *	Public Functions
	 */
	State(TYPE type = UNKNOWN);

	virtual void enter() = 0;

	virtual void handleCanFrame(const Can::Frame& frame) = 0;

protected:
	/*
	 *	Instances
	 */
	Core* core_ = nullptr;

	/*
	 *	Private Variables
	 */
	TYPE type_ = UNKNOWN;

	bool blocked = false;
};
