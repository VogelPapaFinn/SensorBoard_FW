#pragma once

// Project includes
#include "Driver/WifiHost.hpp"
#include "Driver/WifiJoin.hpp"

// espidf includes
#include "esp_http_server.h"

class WebInterface
{
public:
	WebInterface(bool initiateWifiDriver = true);

	void send(int clientFD, const std::string& data) const;

	std::unordered_map<int, std::vector<uint16_t>>& getTrackedSensors();

	SemaphoreHandle_t& getSensorsMutex();

	/*
	 *	Private ISRs
	 */
	esp_err_t websocketHandler(httpd_req_t* p_reqst);

	void websocketCrashed(const int fd);
private:
	/*
	 *	Private Functions
	 */
	 void onConnectedToWifi();

	void sendAllSensors(int clientFD) const;

	/*
	 *	Private Variables
	 */
	Core* core_ = nullptr;

	bool initialized_ = false;

	ArduinoJson::JsonDocument* config_;

	std::string wifiMode_;

	WifiHost wifiHost_;
	WifiJoin wifiJoin_;

	httpd_config_t httpdConfig_ = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t httpdHandle_;

	SemaphoreHandle_t sensorsMutex_;
	std::unordered_map<int, std::vector<uint16_t>> trackedSensors_;
	TaskHandle_t updateSensorsDataTask_;
};
