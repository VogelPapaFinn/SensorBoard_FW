#pragma once

// Project includes
#include "Can.hpp"
#include "Config.hpp"
#include "Driver/Display.hpp"
#include "Driver/Wifi.hpp"

// espidf includes
#include "esp_adc/adc_oneshot.h"

// Circular inclusion
class WebInterface;

/*
 *	Public constexpr
 */
constexpr gpio_num_t GPIO_DISPLAY1 = GPIO_NUM_13;
constexpr gpio_num_t GPIO_DISPLAY2 = GPIO_NUM_14;
constexpr gpio_num_t GPIO_DISPLAY3 = GPIO_NUM_21;

/*
 *	Class
 */
class Core
{
public:
	static Core* get();

	void setMainEventQueue(QueueHandle_t queue);

	QueueHandle_t getMainEventQueue() const;

	std::vector<Display>* getDisplays();

	adc_oneshot_unit_handle_t* getAdc();

	ArduinoJson::JsonDocument* getConfig() const;

	void saveConfig() const;

	void setWifi(Wifi* wifi);

	Wifi* getWifi() const;

	void setWebinterface(WebInterface* web);

	WebInterface* getWebinterface() const;

	/*
	 *	CAN related functions
	 */
	Can* getCan() const;

private:
	/*
	 *	Instances
	 */
	static Core* self_;

	Can* can_ = nullptr;

	Wifi* wifi_ = nullptr;

	WebInterface* webInterface_ = nullptr;

	/*
	 *	Private Functions
	 */
	Core();

	/*
	 *	Private Variables
	 */
	QueueHandle_t mainEventQueue_ = nullptr;

	std::vector<Display> displays_ = {
		Display(GPIO_DISPLAY1, CAN_MASTER_ID + 1, 1, true),
		Display(GPIO_DISPLAY2, CAN_MASTER_ID + 2, 0, false),
		Display(GPIO_DISPLAY3, CAN_MASTER_ID + 3, 2, true),
	};

	adc_oneshot_unit_handle_t adc1Handle_;

	Filesystem* filesystem_ = nullptr;

	Config* config_ = nullptr;

	ArduinoJson::JsonDocument* jsonConfig_ = nullptr;
};
