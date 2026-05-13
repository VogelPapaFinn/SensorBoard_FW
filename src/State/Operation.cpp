#include "State/Operation.hpp"

// Project includes
#include "Sensor/FuelLevel.hpp"
#include "Sensor/OilPressure.hpp"
#include "Sensor/WaterTemperature.hpp"
#include "Sensor/Rpm.hpp"
#include "Sensor/Speed.hpp"
#include "Sensor/LeftIndicator.hpp"
#include "Sensor/RightIndicator.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Operation";

constexpr auto PASSIVE_SENSOR_POLL_HZ = 10;
constexpr auto BROADCAST_SENSOR_DATA_HZ = 1;

/*
 *	Private Static Task
 */
void staticReadPassiveSensorsTask(void* param)
{
	if (param == nullptr) {
		return;
	}

	Operation* instance = static_cast<Operation*>(param);

	instance->readPassiveSensorsTask();
}

void staticBroadcastSensorDataTask(void* param)
{
	if (param == nullptr) {
		return;
	}

	Operation* instance = static_cast<Operation*>(param);

	instance->broadcastSensorsTask();
}

/*
 *	Public Function implementations
 */
Operation::Operation() :
	State(State::OPERATION), readPassiveSensorsTaskHandle_(nullptr), broadCastSensorDataTaskHandle_(nullptr)
{
	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to install the ISR service");
	}
}

Operation::~Operation()
{
	vTaskDelete(readPassiveSensorsTaskHandle_);
	vTaskDelete(broadCastSensorDataTaskHandle_);

	for (auto& sensor : passiveSensor_) {
		free(sensor);
	}

	for (auto& sensor : activeSensor_) {
		sensor->disable();
		free(sensor);
	}
}

void Operation::enter()
{
	const auto adc1Handle = core_->getAdc();
	passiveSensor_ = {
		new FuelLevel(adc1Handle),
		new OilPressure(adc1Handle),
		new WaterTemperature(adc1Handle),
	};

	activeSensor_ = {
		new Rpm(),
		new Speed(),
		new LeftIndicator(),
		new RightIndicator(),
	};
	for (const auto& sensor : activeSensor_) {
		sensor->enable();
	}

	if (xTaskCreate(staticReadPassiveSensorsTask, "OperationReadPassiveSensorsTask", 2048, this, 2,
	                &readPassiveSensorsTaskHandle_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create task for reading all passive HW sensors");
	}

	if (xTaskCreate(staticBroadcastSensorDataTask, "OperationBroadcastSensorDataTask", 2048, this, 2,
	                &broadCastSensorDataTaskHandle_) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create task for broadcasting sensor data");
	}
}

void Operation::handleCanFrame(const Can::Frame& frame)
{
}

void Operation::readPassiveSensorsTask() const
{
	while (true) {
		for (auto& sensor : passiveSensor_) {
			sensor->read();
		}

		vTaskDelay(pdMS_TO_TICKS(1000 / PASSIVE_SENSOR_POLL_HZ));
	}
}

static bool b = false;
void Operation::broadcastSensorsTask() const
{
	while (true) {
		Can::Frame frame;
		frame.sender = CAN_MASTER_ID;
		frame.target = CAN_BROADCAST_ID;
		frame.group = CanFrame::GROUP::SENSOR;
		frame.function = CanFrame::SENSOR::BROADCAST_DATA;
		frame.dataLengthCode = 7;
		frame.answer = false;

		for (uint8_t i = 0; i < passiveSensor_.size(); i++) {
			frame.data[i] = passiveSensor_.at(i)->get();
		}
		for (uint8_t i = passiveSensor_.size(); i < passiveSensor_.size() + activeSensor_.size(); i++) {
			frame.data[i] = activeSensor_.at(i - passiveSensor_.size())->get();
		}

		frame.data[4] = rand() % 101;
		if (b) {
			b = false;
		frame.data[6] = 1;
		} else {
			b = true;
		frame.data[6] = 0;
		}

		core_->getCan()->queueFrame(frame);

		vTaskDelay(pdMS_TO_TICKS(1000 / BROADCAST_SENSOR_DATA_HZ));
	}
}