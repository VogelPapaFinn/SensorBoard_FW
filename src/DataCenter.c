#include "DataCenter.h"

// C includes
#include <string.h>

// espidf includes
#include "../../esp-idf/components/json/cJSON/cJSON.h"


/*
 *	Private variables
 */

//! \brief A list of FreeRTOS Queues that should be notified once the data changes
static QueueHandle_t** queuesToNotify_ = NULL;

//! \brief The amount of registered FreeRTOS queue handles
static uint8_t amountOfQueuesToNotifyReceived_ = 0;

//! \brief Array which holds the data of the displays
Display_t displays_[AMOUNT_OF_DISPLAYS];

/*
 *	Private Functions
 */

void broadcast(QUEUE_EVENT_T event)
{
	// Notify all queues
	for (uint8_t i = 0; i < amountOfQueuesToNotifyReceived_; i++) {
		xQueueSend(*queuesToNotify_[i], &event, portMAX_DELAY);
	}
}

/*
 *	Public Functions
 */
void initDataCenter()
{
	// Initialize all displays
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		displays_[i].connected = false;
		displays_[i].firmwareVersion = NULL;
		displays_[i].uuid = NULL;
		displays_[i].comId = 255;
		displays_[i].wifiStatus = NULL;
	}
}

bool registerDataCenterCbQueue(QueueHandle_t* queueHandle)
{
	// Make the array larger
	const void* newAddr = realloc(queuesToNotify_, sizeof(QueueHandle_t*) * (amountOfQueuesToNotifyReceived_ + 1));

	// Did it work?
	if (newAddr != NULL) {
		queuesToNotify_ = (QueueHandle_t**)newAddr;
	}
	else {
		return false;
	}

	// Increase the counter
	amountOfQueuesToNotifyReceived_++;

	// Then save the task handle
	queuesToNotify_[amountOfQueuesToNotifyReceived_ - 1] = queueHandle;

	return true;
}

void broadcastSensorDataChanged()
{
	// Build the event
	QUEUE_EVENT_T event;
	event.command = QUEUE_SENSOR_DATA_CHANGED;

	// Notify everybody
	broadcast(event);
}

void broadcastDisplayStatiChanged()
{
	// Build the event
	QUEUE_EVENT_T event;
	event.command = QUEUE_DISPLAY_STATI_CHANGED;

	// Notify everybody
	broadcast(event);
}

Display_t* getDisplayStatiObjects()
{
	return displays_;
}

char* getAllDisplayStatiAsJSON(void)
{
	// Create the JSON packet
	cJSON* rootJson = cJSON_CreateObject();
	cJSON_AddStringToObject(rootJson, "type", "DISPLAY_STATI");

	// Add all displays
	cJSON* displays = cJSON_AddArrayToObject(rootJson, "displays");
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		cJSON* display = cJSON_CreateObject();
		cJSON_AddBoolToObject(display, "connected", displays_[i].connected);
		cJSON_AddStringToObject(display, "firmware", displays_[i].firmwareVersion);
		cJSON_AddStringToObject(display, "uuid", displays_[i].uuid);
		cJSON_AddNumberToObject(display, "com_id", displays_[i].comId);
		cJSON_AddStringToObject(display, "wifi", displays_[i].wifiStatus);
		cJSON_AddItemToArray(displays, display);
	}

	// Get the unformatted string
	char* unformatted = cJSON_PrintUnformatted(rootJson);
	// Free memory
	cJSON_Delete(rootJson);

	// Return the length of the array
	return unformatted;
}
