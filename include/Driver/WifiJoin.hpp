#pragma once

// C++ includes
#include <functional>

// Project includes
#include "Config.hpp"
#include "Core.hpp"

// espidf includes
#include "esp_wifi.h"

class WifiJoin
{
public:
	WifiJoin();

	~WifiJoin();

	void callOnConnect(const std::function<void()>& cb);

	bool connect();

	void disconnect();

	/*
	 *	Private ISR
	 */
	void ipEventHandler(const esp_event_base_t p_eventBase, const int32_t eventId, void* p_eventData);
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
	bool connected_ = false;

	std::string ssid_ = "";
	std::string password_ = "";

	esp_netif_t* espNetif_ = nullptr;
	wifi_init_config_t wifiInitConfig_ = WIFI_INIT_CONFIG_DEFAULT();
	wifi_config_t wifiConfig_;

	std::function<void()> onConnectedCb_;

	esp_event_handler_instance_t wifiEventHandlerInstance_;
	esp_event_handler_instance_t ipEventHandlerInstance_;

	std::array<uint8_t, 4> ip_ = {};
};
