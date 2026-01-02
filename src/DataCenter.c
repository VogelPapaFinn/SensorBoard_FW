#include "DataCenter.h"

// C includes
#include <string.h>

// espidf includes
#include "../../esp-idf/components/json/cJSON/cJSON.h"


/*
 *	Private variables
 */

//! \brief A list of FreeRTOS Queues that should be notified once the data changes
static QueueHandle_t** g_queuesToNotify = NULL;

//! \brief The amount of registered FreeRTOS queue handles
static uint8_t g_amountOfQueuesToNotifyReceived = 0;

//! \brief Array which holds the data of the displays
Display_t g_displays[AMOUNT_OF_DISPLAYS];

/*
 *	Private Functions
 */

void broadcast(QueueEvent_t event)
{
	// Notify all queues
	for (uint8_t i = 0; i < g_amountOfQueuesToNotifyReceived; i++) {
		xQueueSend(*g_queuesToNotify[i], &event, portMAX_DELAY);
	}
}

/*
 *	External Variables
 */
uint8_t g_amountOfConnectedDisplays = 0;

/*
 *	Public Functions
 */
void dataCenterInit()
{
	g_amountOfConnectedDisplays = 0;

	// Initialize all displays
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		g_displays[i].connected = false;
		g_displays[i].firmwareVersion = NULL;
		g_displays[i].uuid = NULL;
		g_displays[i].comId = 255;
		g_displays[i].wifiStatus = NULL;
	}
}

bool registerDataCenterCbQueue(QueueHandle_t* p_queueHandle)
{
	// Make the array larger
	const void* newAddr = realloc((void*)g_queuesToNotify, sizeof(QueueHandle_t*) * (g_amountOfQueuesToNotifyReceived + 1));

	// Did it work?
	if (newAddr != NULL) {
		g_queuesToNotify = (QueueHandle_t**)newAddr;
	}
	else {
		return false;
	}

	// Increase the counter
	g_amountOfQueuesToNotifyReceived++;

	// Then save the task handle
	g_queuesToNotify[g_amountOfQueuesToNotifyReceived - 1] = p_queueHandle;

	return true;
}

void broadcastSensorDataChanged()
{
	// Build the event
	QueueEvent_t event;
	event.command = QUEUE_SENSOR_DATA_CHANGED;

	// Notify everybody
	broadcast(event);
}

void broadcastDisplayStatiChanged()
{
	// Build the event
	QueueEvent_t event;
	event.command = QUEUE_DISPLAY_STATI_CHANGED;

	// Notify everybody
	broadcast(event);
}

Display_t* getDisplayStatiObjects()
{
	return g_displays;
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
		cJSON_AddBoolToObject(display, "connected", g_displays[i].connected);
		cJSON_AddStringToObject(display, "firmware", g_displays[i].firmwareVersion);
		cJSON_AddStringToObject(display, "uuid", g_displays[i].uuid);
		cJSON_AddNumberToObject(display, "com_id", g_displays[i].comId);
		cJSON_AddStringToObject(display, "wifi", g_displays[i].wifiStatus);
		cJSON_AddItemToArray(displays, display);
	}

	// Get the unformatted string
	char* unformatted = cJSON_PrintUnformatted(rootJson);
	// Free memory
	cJSON_Delete(rootJson);

	// Return the length of the array
	return unformatted;
}
