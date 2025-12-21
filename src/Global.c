// Project includes
#include "Global.h"
#include "logger.h"

QueueHandle_t updateEventQueue = NULL;

QueueHandle_t mainEventQueue = NULL;

uint64_t knownHwUUIDs[AMOUNT_OF_DISPLAYS] = {0x00};

bool createEventQueues()
{
	// Create the event Queue for the Update Handler
	updateEventQueue = xQueueCreate(20, sizeof(QUEUE_EVENT_T));
	if (updateEventQueue == NULL) {
		loggerCritical("Couldn't create updateEventQueue");

		return false;
	}

	// Create the main Queue for the GUI
	mainEventQueue = xQueueCreate(5, sizeof(QUEUE_EVENT_T));
	if (mainEventQueue == NULL) {
		loggerCritical("Couldn't create mainEventQueue");

		return false;
	}


	// Logging
	loggerInfo("Created event queues");

	return true;
}
