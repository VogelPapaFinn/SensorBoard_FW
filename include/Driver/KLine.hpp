#pragma once

// Project includes
#include "EcuSensors.hpp"

// C++ includes
#include <vector>

// espidf includes
#include "freertos/FreeRTOS.h"

class KLine
{
public:
	KLine();

	~KLine();

	void readEcuId();

	void readPid(uint16_t pid);

	QueueHandle_t getQueue() const;

	SemaphoreHandle_t getLastMessageMutex() const;

	uint8_t getLastMessageLength() const;

	uint8_t* getLastMessage();
private:
	/*
	 *	Private Functions
	 */
	void send(uint8_t* data, const uint8_t length, const bool calcChecksum = true);

	static uint8_t calculateChecksum(const uint8_t* data, const uint8_t& length);

	/*
	 *	Private Variables
	 */
	bool initialized_ = false;

	QueueHandle_t uartQueueHandle_ = nullptr;

	TaskHandle_t rxTaskHandle_ = nullptr;

	uint8_t lastMessage_[16] = {0x00};
	uint8_t lastMessageLength_ = 0;
	SemaphoreHandle_t lastMessageMutex_ = nullptr;
};
