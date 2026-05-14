// Project includes
#include "Can.hpp"
#include "Driver/Display.hpp"
#include "Filesystem.hpp"
#include "Core.hpp"
#include "State/Registration.hpp"
#include "Event.hpp"
#include "State/Operation.hpp"

// espidf includes
#include <freertos/FreeRTOS.h>

/*
 *	constexpr
 */
constexpr auto TAG = "main";

/*
 *	Private Static Variables
 */
static Core* core = nullptr;

static QueueHandle_t canQueueHandle = xQueueCreate(10, sizeof(Can::Frame));

static QueueHandle_t mainEventQueueHandle = xQueueCreate(20, sizeof(Event));

static std::shared_ptr<State> currentState;

/*
 *	Can rx callback function
 */
static void canRxTask(void* param)
{
	Can::Frame rxFrame;
	while (true) {
		if (xQueueReceive(canQueueHandle, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		currentState->handleCanFrame(rxFrame);
	}
}

static void mainEventTask(void* param)
{
	Event event;
	while (true) {
		if (xQueueReceive(mainEventQueueHandle, &event, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Act depending on the event
		switch (event.type) {
			case Event::REGISTRATION_FINISHED:
			{
				currentState = std::make_shared<Operation>();
				currentState->enter();
			} break;
			default: ;
		}
	}
}

/*
 *	main function
 */
#include "Driver/WifiJoin.hpp"
extern "C" void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(500));

	WifiJoin w;
	w.connect();

	while (true) {

		vTaskDelay(pdMS_TO_TICKS(2000));
	}


	ESP_LOGI(TAG, "--- --- --- --- --- --- ---");
	ESP_LOGI(TAG, "Startup");

	Filesystem* filesystem = Filesystem::get();

	core = Core::get();
	core->setMainEventQueue(mainEventQueueHandle);
	core->getCan()->registerRxCbQueue(&canQueueHandle);

	if (xTaskCreate(canRxTask, "MainCanRxTask", 2048, NULL, 2, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create CAN RX Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	if (xTaskCreate(mainEventTask, "MainEventTask", 2048, NULL, 2, NULL) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create main event task");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	currentState = std::make_shared<Registration>();
	currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
