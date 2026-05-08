// Project includes
#include "Can.hpp"
#include "Driver/Display.hpp"
#include "Filesystem.hpp"
#include "Core.hpp"
#include "State/Registration.hpp"

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

/*
 *	main function
 */
extern "C" void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(500));

	ESP_LOGI(TAG, "--- --- --- --- --- --- ---");
	ESP_LOGI(TAG, "Startup");

	Filesystem* filesystem = Filesystem::get();

	core = Core::get();
	core->getCan()->registerRxCbQueue(&canQueueHandle);

	TaskHandle_t canRxTaskHandle;
	if (xTaskCreate(canRxTask, "MainCanRxTask", 2048 * 4, NULL, 2, &canRxTaskHandle) != pdPASS) {
		ESP_LOGE(TAG, "Failed to create CAN RX Task. Restarting...");
		esp_restart();
		vTaskDelay(pdMS_TO_TICKS(100000)); // Fallback
	}

	currentState = std::make_shared<Registration>();
	currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
