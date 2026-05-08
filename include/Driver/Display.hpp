#pragma once

// C++ includes
#include <thread>
#include <vector>

// espidf includes
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

class Display
{
public:
	explicit Display(gpio_num_t powerGpio, const uint8_t& canId);

	uint8_t getCanId() const;

	void turnOn() const;

	void turnOff() const;

private:
	uint8_t canId_ : 3 = 0;

	gpio_num_t powerGpio_ = GPIO_NUM_NC;

	static std::vector<uint8_t> g_possibleCanIds;

	TaskHandle_t giveCanIdReceiveTask_;

	TaskHandle_t giveCanIdSendTask_;

	QueueHandle_t canQueue_;
};

