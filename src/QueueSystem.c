// Project includes
#include "QueueSystem.h"
#include "esp_log.h"

/*
 *	Global variables
 */
QueueHandle_t g_updateEventQueue = NULL;
QueueHandle_t g_mainEventQueue = NULL;

/*
 *	Public functions
 */
bool createEventQueues()
{
	// Create the event Queue for the Update Handler
	g_updateEventQueue = xQueueCreate(20, sizeof(QueueEvent_t));
	if (g_updateEventQueue == NULL) {
		ESP_LOGE("QueueSystem", "Couldn't create updateEventQueue"); // NOLINT

		return false;
	}

	// Create the main Queue for the GUI
	g_mainEventQueue = xQueueCreate(5, sizeof(QueueEvent_t));
	if (g_mainEventQueue == NULL) {
		ESP_LOGE("QueueSystem", "Couldn't create mainEventQueue");

		return false;
	}


	// Logging
	ESP_LOGI("QueueSystem", "Created event queues");

	return true;
}
