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
	Operation();

	~Operation();

	void enter() override;

	void handleCanFrame(const Can::Frame& frame) override;

	void executeDisplayUpdate(const uint8_t displayId) const;

	/*
	 *	Private Tasks
	 */
	void readPassiveSensorsTask() const;

	void broadcastSensorsTask() const;

	void logSensorsTask() const;

private:
	/*
	 *	Private Functions
	 */
	void setupDisplayWifi() const;

	/*
	 *	Instances
	 */
	Filesystem* filesystem_;

	/*
	 *	Private Variables
	 */
	bool simulation_ = false;
	std::vector<std::array<uint8_t, 8>> simulationData_;

	TaskHandle_t readPassiveSensorsTaskHandle_;

	TaskHandle_t broadCastSensorDataTaskHandle_;

	std::vector<PassiveSensor*> passiveSensors_;

	std::vector<ActiveSensor*> activeSensors_;

	std::vector<EcuSensor*> ecuSensors_;

	ArduinoJson::JsonDocument* config_ = nullptr;

	TaskHandle_t logSensorDataTaskHandle_;

	FILE* sensorDataCsv_ = nullptr;

	KLine* kline_ = nullptr;
};
