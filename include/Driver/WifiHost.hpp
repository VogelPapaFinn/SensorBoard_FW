#pragma once

// Project includes
#include "Config.hpp"
#include "Core.hpp"

// espidf includes
#include "esp_wifi.h"

class WifiHost
{
public:
	WifiHost();

	~WifiHost();

	bool start();

	void stop();

private:
	/*
	 *	Instances
	 */
	Core* core_ = nullptr;

	ArduinoJson::JsonDocument* config_ = nullptr;

	/*
	 *	Variables
	 */
	bool active_ = false;

	std::string ssid_ = "";
	std::string password_ = "";

	esp_netif_t* espNetif_ = nullptr;
	wifi_init_config_t wifiInitConfig_ = WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t wifiConfig_;
};
