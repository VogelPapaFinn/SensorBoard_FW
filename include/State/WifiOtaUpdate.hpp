#pragma once

// Project includes
#include "State/State.hpp"

class WifiOtaUpdate : public State
{
public:
	WifiOtaUpdate();

	~WifiOtaUpdate();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

private:
	void broadcastWifiSSID();
};
