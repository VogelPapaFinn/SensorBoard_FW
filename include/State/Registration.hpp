#pragma once

// Project include
#include "State/State.hpp"

class Registration : public State
{
public:
	Registration();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

private:
	/*
	 *	Private Functions
	 */
	void confirmId(const uint8_t& id);

	void setId(const uint8_t& oldId, const uint8_t& newId);

	void nextDisplay();

	void setScreen() const;

	void setRotation() const;

	void displayRegistrationCompleted() const;

	void wakeUpAllDisplays();

	/*
	 *	Private Variables
	 */
	uint8_t currDisplay = 0;
};
