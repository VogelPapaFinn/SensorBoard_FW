#include "State/Operation.hpp"

// Project includes
#include "Sensor/FuelLevel.hpp"
#include "Sensor/LeftIndicator.hpp"
#include "Sensor/OilPressure.hpp"
#include "Sensor/RightIndicator.hpp"
#include "Sensor/Rpm.hpp"
#include "Sensor/Speed.hpp"
#include "Sensor/WaterTemperature.hpp"
#include "Driver/WifiJoin.hpp"
#include "Driver/WifiHost.hpp"
#include "Driver/Wifi.hpp"

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
	config_ = core_->getConfig();

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

	/*
	 *	Wifi
	 */
	Wifi::WIFI_TYPE wifiType = Wifi::WIFI_TYPE::HOST;
	if ((*config_)["ForceWifiMode"]) {
		wifiType = static_cast<Wifi::WIFI_TYPE>((*config_)["ForceWifiMode"].as<int>());
	}

	Wifi* wifi = nullptr;

	// Host
	if (wifiType == Wifi::WIFI_TYPE::HOST) {
		// Create Wifi
		wifi = new WifiHost();
		core_->setWifi(wifi);

		// Initialize Wifi
		wifi->setSSID((*config_)["WifiHost"]["ssid"]);
		wifi->setPassword((*config_)["WifiHost"]["password"]);
		wifi->callOnSuccess([this]
		{
			WebInterface* webInterface = new WebInterface();
			core_->setWebinterface(webInterface);

			this->setupDisplayWifi();
		});

		wifi->start();
	}

	// Join
	else if (wifiType == Wifi::WIFI_TYPE::JOIN) {
		// Create Wifi
		wifi = new WifiJoin();
		core_->setWifi(wifi);

		// Initialize Wifi
		wifi->setSSID((*config_)["WifiJoin"]["ssid"]);
		wifi->setPassword((*config_)["WifiJoin"]["password"]);
		wifi->callOnSuccess([this]
		{
			WebInterface* webInterface = new WebInterface();
			core_->setWebinterface(webInterface);

			this->setupDisplayWifi();
		});

		wifi->start();
	}

	// Error
	else {
		ESP_LOGW(TAG, "Failed to initialize wifi. Continuing without it");
		return;
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

/*
 *	Private Function Implementations
 */
void Operation::setupDisplayWifi() const
{
	ESP_LOGI(TAG, "Starting to transmit SSID and Password to the displays");

	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = CAN_BROADCAST_ID;
	txFrame.group = CanFrame::GROUP::WIFI;
	txFrame.function = CanFrame::WIFI::SET_SSID;

	/*
	 *	SSID
	 */
	// Split the ssid up into packages with a size of max 8 bytes/chars
	const auto& ssid = core_->getWifi()->getSSID();
	std::vector<std::vector<char>> allSsidPackages;
	std::vector<char> ssidPackage;
	for (const auto& c : ssid) {
		if (ssidPackage.size() >= 8) {
			allSsidPackages.push_back(ssidPackage);
			ssidPackage.clear();
		}

		ssidPackage.push_back(c);
	}
	allSsidPackages.push_back(ssidPackage); // Add the last package too

	// Transmit all packages to the displays
	for (const auto& package : allSsidPackages) {
		txFrame.dataLengthCode = package.size();
		std::copy(package.begin(), package.end(), txFrame.data);

		Core::get()->getCan()->queueFrame(txFrame);
	}

	/*
	 *	Password
	 */
	// Split the password up into packages with a size of max 8 bytes/chars
	const auto& password = core_->getWifi()->getPassword();
	std::vector<std::vector<char>> allPsswdPackages;
	std::vector<char> psswdPackage;
	for (const auto& c : password) {
		if (psswdPackage.size() >= 8) {
			allPsswdPackages.push_back(psswdPackage);
			psswdPackage.clear();
		}

		psswdPackage.push_back(c);
	}
	allPsswdPackages.push_back(psswdPackage); // Add the last package too

	// Transmit all packages to the displays
	for (const auto& package : allPsswdPackages) {
		txFrame.dataLengthCode = package.size();
		std::copy(package.begin(), package.end(), txFrame.data);

		Core::get()->getCan()->queueFrame(txFrame);
	}
}
