#pragma once

// Project includes
#include "State/State.hpp"
#include "Sensor/ActiveSensor.hpp"
#include "Sensor/PassiveSensor.hpp"
#include "WebInterface/WebInterface.hpp"

class Operation : public State
{
public:
	Operation();

	~Operation();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

	/*
	 *	Private Tasks
	 */
	void readPassiveSensorsTask() const;

	void broadcastSensorsTask() const;

private:
	/*
	 *	Private Variables
	 */
	TaskHandle_t readPassiveSensorsTaskHandle_;

	TaskHandle_t broadCastSensorDataTaskHandle_;

	std::vector<PassiveSensor*> passiveSensor_;

	std::vector<ActiveSensor*> activeSensor_;
};
