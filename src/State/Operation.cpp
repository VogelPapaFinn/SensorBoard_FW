#include "State/Operation.hpp"

// Project includes
#include "Sensor/FuelLevel.hpp"
#include "Sensor/LeftIndicator.hpp"
#include "Sensor/OilPressure.hpp"
#include "Sensor/RightIndicator.hpp"
#include "Sensor/Rpm.hpp"
#include "Sensor/Speed.hpp"
#include "Sensor/WaterTemperature.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Operation";

constexpr auto PASSIVE_SENSOR_POLL_HZ = 10;
constexpr auto BROADCAST_SENSOR_DATA_HZ = 100;

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
	/*
	 *	Setup passive sensors
	 */
	const auto adc1Handle = core_->getAdc();
	passiveSensor_ = {
		new FuelLevel(adc1Handle),
		new OilPressure(adc1Handle),
		new WaterTemperature(adc1Handle),
	};

	/*
	 *	Setup active sensors
	 */
	activeSensor_ = {
		new Rpm(),
		new Speed(),
		new LeftIndicator(),
		new RightIndicator(),
	};
	for (const auto& sensor : activeSensor_) {
		sensor->enable();
	}

	/*
	 *	Setup read & broadcast task
	 */
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

// #include "esp_random.h"
void Operation::broadcastSensorsTask() const
{
	Can::Frame frame;
	frame.sender = CAN_MASTER_ID;
	frame.target = CAN_BROADCAST_ID;
	frame.group = CanFrame::GROUP::SENSOR;
	frame.function = CanFrame::SENSOR::BROADCAST_DATA;
	frame.dataLengthCode = 8;
	frame.answer = false;

	uint8_t lastData[8] = {0x00};

	while (true) {
		// Fuel Level, Oil Pressure, Water Temperature
		for (uint8_t i = 0; i < passiveSensor_.size(); i++) {
			frame.data[i] = passiveSensor_.at(i)->get();
		}
		// frame.data[0] = static_cast<uint8_t>(esp_random() % 101);

		// RPM
		const auto& rpm = activeSensor_.at(0)->get();
		frame.data[3] = rpm >> 8;
		frame.data[4] = rpm & 0xFF;

		// Speed
		frame.data[5] = activeSensor_.at(1)->get();

		// Left Indicator
		frame.data[6] = activeSensor_.at(2)->get();

		// Right Indicator
		frame.data[7] = activeSensor_.at(3)->get();

		// Did the data stay the same?
		bool equal = true;
		for (uint8_t i = 0; i < frame.dataLengthCode; i++) {
			equal &= frame.data[i] == lastData[i];
		}

		if (equal) {
			core_->getCan()->queueFrame(frame);
			memcpy(lastData, frame.data, frame.dataLengthCode);
		}

		vTaskDelay(pdMS_TO_TICKS(1000 / BROADCAST_SENSOR_DATA_HZ));
	}
}
