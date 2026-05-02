#include "Driver/Display.hpp"

// Project includes
#include "Global.h"
#include "Can.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	Static Member Variables
 */
std::vector<uint8_t> Display::g_possibleCanIds = {1, 2, 3};

/*
 *	constexpr
 */
constexpr auto TAG = "Display";

/*
 *	Private Task implementations
 */
void Display::giveCanIdReceiveTask(void* p_param)
{
	// Wait for new queue events
	/*TwaiFrame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_operationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Get the message id
		const uint8_t messageId = rxFrame.espidfFrame.header.id >> CAN_FRAME_ID_OFFSET;

		// Get the sender com id
		const uint32_t senderId = (uint8_t)rxFrame.espidfFrame.header.id;

		// Is it of the display we update?
		if (senderId != *(uint8_t*)p_param) {
			continue;
		}
	}*/
}

void Display::giveCanIdSendTask(void* p_param)
{
	// Build CAN frame
	// TwaiFrame_t frame;
	// const CanGroup_t group = GROUP_CONFIGURATION;
	// const CanGroupFunction_t function = FUNCTION_0;
	// canInitiateFrame(&frame, (group << 4) + function, 1);
	// frame.buffer[0] = canId_;
}


/*
 *	Public function implementations
*/
Display::Display(const gpio_num_t powerGpio)
{
	// Save the power GPIO & configure it
	powerGpio_ = powerGpio;
	if (powerGpio_ != GPIO_NUM_NC) {
		gpio_set_direction(powerGpio_, GPIO_MODE_OUTPUT);
	}
}

void Display::supplyPower() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, GPIO_HIGH);
}

void Display::shutdownPower() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, GPIO_LOW);
}

void Display::giveCanId()
{
	// TODO: Implement logic to give a Display its ID

	if (g_possibleCanIds.empty()) {
		ESP_LOGE(TAG, "All CAN ID's are already occupied");
		return;
	}

	// if (!canIsReadyTransmit()) {
	// 	ESP_LOGW(TAG, "Can't give Can ID. Can not ready for transmitting");
	// 	return;
	// }

	// Get an available can id
	if (canId_ != 0) {
		canId_ = g_possibleCanIds.back();
		g_possibleCanIds.pop_back();
		ESP_LOGD(TAG, "Occupied CAN ID: %d", canId_);
	}

	// if (!canRegisterRxCbQueue(&canQueue_)) {
	// 	ESP_LOGW(TAG, "Couldn't register CAN callback queue");
	// }

	// Start the can task
	// if (xTaskCreate(giveCanIdReceiveTask, "DisplayGiveCanIdReceiveTask", 2048 * 4, nullptr, 2, &giveCanIdReceiveTask_) != pdPASS) {
	// 	canUnregisterRxCbQueue(&canQueue_);
	// 	ESP_LOGE("CanUpdateManager", "Couldn't create CAN task!");
	// 	return;
	// }

	// Start the send task
	// if (xTaskCreate(giveCanIdSendTask, "DisplayGiveCanIdSendTask", 2048 * 4, nullptr, 2, &giveCanIdSendTask_) != pdPASS) {
	// 	// Cleanup
	// 	vTaskDelete(giveCanIdReceiveTask_);
	// 	canUnregisterRxCbQueue(&canQueue_);
//
	// 	ESP_LOGE("CanUpdateManager", "Couldn't create CAN send task!");
	// 	return;
	// }
}
