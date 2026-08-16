#pragma once

// C++ includes
#include "fstream"

// Project includes
#include "Driver/KLine.hpp"
#include "Sensor/ActiveSensor.hpp"
#include "Sensor/PassiveSensor.hpp"
#include "State/State.hpp"
#include "WebInterface/WebInterface.hpp"

class Operation : public State
{
public:
	Operation(SystemContext* p_sysCon);

	~Operation();

	void enter() override;

	void executeDisplayUpdate(const uint8_t displayId) const;

private:
	/*
	 *	Private Functions
	 */
	void registerToEvents();

	void handleCanFrame(const Can::Frame* frame) const;

	void setupPassiveSensorReadings();

	void setupSensorBroadcasting();

	void setupSensorDataLogging();

	void setupWifi() const;

	void logSensorData() const;

	void connectDisplaysToWifi() const;

	/*
	 *	Private Variables
	 */
	SystemContext* sysCon_ = nullptr;

	std::vector<Sensor*> sensors_;
	std::unordered_map<PassiveSensor*, TimerHandle_t> passiveSensorTimers_;

	TimerHandle_t sensorDataLoggingTimer_ = nullptr;

	std::vector<EcuSensor*> ecuSensors_;

	FILE* sensorDataCsv_ = nullptr;
};
