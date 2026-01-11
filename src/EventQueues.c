// Project includes
#include "EventQueues.h"

// espidf includes
#include <esp_log.h>

/*
 *	Private defines
 */
#define QUEUE_SIZE_CAN 10
#define QUEUE_SIZE_EVENT 10

/*
 *	CAN queues
 */
QueueHandle_t g_registrationManagerCanQueue = NULL;
QueueHandle_t g_operationManagerCanQueue = NULL;
QueueHandle_t g_displayManagerEventQueue = NULL;
QueueHandle_t g_canUpdateManagerCanQueue = NULL;

/*
 *	Event queues
 */
QueueHandle_t g_mainEventQueue = NULL;
QueueHandle_t g_operationManagerEventQueue = NULL;
QueueHandle_t g_canUpdateManagerEventQueue = NULL;


/*
 *	Public functions implementations
 */
bool createEventQueues()
{
	/*
	 *	CAN queues
	 */
	// registration manager queue
	g_registrationManagerCanQueue = xQueueCreate(QUEUE_SIZE_CAN, sizeof(twai_frame_t));
	if (g_registrationManagerCanQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create CAN queue for the registration manager");

		return false;
	}

	// operation manager queue
	g_operationManagerCanQueue = xQueueCreate(QUEUE_SIZE_CAN, sizeof(twai_frame_t));
	if (g_operationManagerCanQueue == 0) {
		ESP_LOGE("EventQueues", "Couldn't create CAN queue for the operation manager");

		return false;
	}

	// can update manager queue
	g_canUpdateManagerCanQueue = xQueueCreate(QUEUE_SIZE_CAN, sizeof(twai_frame_t));
	if (g_canUpdateManagerCanQueue == NULL) {
		ESP_LOGE("EventQueues", "Couldn't create CAN queue for the can update manager");

		return false;
	}

	// Logging
	ESP_LOGI("EventQueue", "Created CAN queues");

	/*
	 *	Event queues
	*/
	// main queue
	g_mainEventQueue = xQueueCreate(QUEUE_SIZE_EVENT, sizeof(QueueEvent_t));
	if (g_mainEventQueue == NULL) {
		ESP_LOGE("EventQueue", "Couldn't create event queue for the main loop");

		return false;
	}

	// operation manager event queue
	g_operationManagerEventQueue = xQueueCreate(QUEUE_SIZE_EVENT, sizeof(QueueEvent_t));
	if (g_operationManagerEventQueue == NULL) {
		ESP_LOGE("EventQueue", "Couldn't create event queue for the operation manager");

		return false;
	}

	// can update manager event queue
	g_canUpdateManagerEventQueue = xQueueCreate(QUEUE_SIZE_EVENT, sizeof(QueueEvent_t));
	if (g_canUpdateManagerEventQueue == NULL) {
		ESP_LOGE("EventQueue", "Couldn't create event queue for the can update manager");

		return false;
	}

	// Logging
	ESP_LOGI("EventQueue", "Created event queues");

	return true;
}
