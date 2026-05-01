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
	explicit Display(gpio_num_t powerGpio);

	void supplyPower() const;

	void shutdownPower() const;

	void giveCanId();

private:
	/*
	 *	Private Tasks
	 */
	void giveCanIdReceiveTask(void* p_param);

	void giveCanIdSendTask(void* p_param);

	uint8_t canId_ : 3 = 0;

	gpio_num_t powerGpio_ = GPIO_NUM_NC;

	static std::vector<uint8_t> g_possibleCanIds;

	TaskHandle_t giveCanIdReceiveTask_;

	TaskHandle_t giveCanIdSendTask_;

	QueueHandle_t canQueue_;
};

