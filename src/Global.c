// Project includes
#include "Global.h"
#include "logger.h"

QueueHandle_t g_updateEventQueue = NULL;

QueueHandle_t g_mainEventQueue = NULL;

bool createEventQueues()
{
	// Create the event Queue for the Update Handler
	g_updateEventQueue = xQueueCreate(20, sizeof(QueueEvent_t));
	if (g_updateEventQueue == NULL) {
		loggerCritical("Couldn't create updateEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	g_mainEventQueue = xQueueCreate(5, sizeof(QueueEvent_t));
	if (g_mainEventQueue == NULL) {
		loggerCritical("Couldn't create mainEventQueue");

		return false;
	}


	// Logging
	loggerInfo("Created event queues");

	return true;
}
