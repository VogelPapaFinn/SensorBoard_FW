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

	/*
	 *	Private ISRs
	 */
	esp_err_t websocketHandler(httpd_req_t* p_reqst);
private:
	/*
	 *	Private Functions
	 */
	 void onConnectedToWifi();

	void send(int clientFD, const std::string& data) const;

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
};
