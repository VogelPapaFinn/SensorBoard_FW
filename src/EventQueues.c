// Project includes
#include "EventQueues.h"

// espidf includes
#include <esp_log.h>

/*
 *	Public variables
 */
QueueHandle_t g_displayManagerEventQueue = NULL;

QueueHandle_t g_mainEventQueue = NULL;

/*
 *	Public functions implementations
 */
bool createEventQueues()
{
	// Create the event Queue for the Display Manager
	g_displayManagerEventQueue = xQueueCreate(10, sizeof(QueueEvent_t));
	if (g_displayManagerEventQueue == NULL) {
		ESP_LOGE("EventQueue", "Couldn't create displayManagerEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	g_mainEventQueue = xQueueCreate(10, sizeof(QueueEvent_t));
	if (g_mainEventQueue == NULL) {
		ESP_LOGE("EventQueue", "Couldn't create mainEventQueue");

		return false;
	}


	// Logging
	ESP_LOGI("EventQueue", "Created event queues");

	return true;
}
