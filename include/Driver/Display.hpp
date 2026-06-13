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
	explicit Display(gpio_num_t powerGpio, const uint8_t& canId, const uint8_t& screen, const bool& rotateBy180);

	uint8_t getCanId() const;

	uint8_t getScreen() const;

	bool isRotated() const;

	void turnOn() const;

	void turnOff() const;

private:
	uint8_t canId_ = 0;

	uint8_t screen_ = 0;

	bool rotated_ = false;

	gpio_num_t powerGpio_ = GPIO_NUM_NC;

	static std::vector<uint8_t> g_possibleCanIds;

	TaskHandle_t giveCanIdReceiveTask_;

	TaskHandle_t giveCanIdSendTask_;

	QueueHandle_t canQueue_;
};

