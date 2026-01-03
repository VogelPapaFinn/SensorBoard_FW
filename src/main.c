// Project includes
#include "FilesystemDriver.h"
#include "EventQueues.h"
#include "SensorManager.h"
#include "../include/WebInterface/WebInterface.h"
#include "can.h"
#include "can_messages.h"
#include "statemachine.h"
#include "ConfigManager.h"
#include "DisplayManager.h"

// espidf includes
#include <esp_mac.h>
#include <esp_timer.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include "../../esp-idf/components/json/cJSON/cJSON.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

/*
 *	Defines
 */
// Config file
#define WIFI_CONFIG_NAME "wifi_config.json"

/*
 *	Prototypes
 */
void handleCanMessage(const QueueEvent_t* p_queueEvent);
void initOperationMode(const QueueEvent_t* p_queueEvent);
void readSensorData(const QueueEvent_t* p_queueEvent);
void handleSensorDataChanged(const QueueEvent_t* p_queueEvent);
void handleDisplayStatiChanged(const QueueEvent_t* p_queueEvent);

void registrationTimerCallback(void* p_arg);
void readSensorDataISR(void* p_arg);
void sendUUIDRequest(void);

void debugListAllSpiffsFiles();
/*
 *	Private typedefs
 */
//! \brief A typedef enum indicating the state we are in
typedef enum
{
	STATE_INIT,
	STATE_OPERATION,
} State_t;

typedef void (*EventHandlerFunction_t)(const QueueEvent_t* p_queueEvent);

/*
 *	Private Variables
 */
const uint8_t g_ownCanSenderId = 0x00;
static const EventHandlerFunction_t g_eventHandlers[] = {
	[RECEIVED_NEW_CAN_MESSAGE] = handleCanMessage,
	[REQUEST_UUID] = displayStartRegistrationProcess,
	[INIT_OPERATION_MODE] = initOperationMode,
};
const uint8_t g_amountOfEventHandlers = sizeof(g_eventHandlers) / sizeof(EventHandlerFunction_t);

static bool g_operationModeInitialized = false;


/*
 *	Interrupt Service Routines
 */


/*
 *	Main functions
 */
void app_main(void) // NOLINT - Deactivate clang-tiny for this line
{
	// Initialize the File Manager
	filesystemInit();

	// Create the event queues
	createEventQueues();

	// Initialize the Sensor Manager
	sensorManagerInit();

	// Initialize the Display Manager
	displayManagerInit();

	// Initialize the can node
	initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Register the queue to the CAN bus
	registerCanRxCbQueue(&g_mainEventQueue);

	// Send a UUID Request
	QueueEvent_t initRequest;
	initRequest.command = REQUEST_UUID;
	xQueueSend(g_mainEventQueue, &initRequest, portMAX_DELAY);

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Act depending on the event
			if (queueEvent.command < g_amountOfEventHandlers && g_eventHandlers[queueEvent.command] != NULL) {
				// Aufruf des Handlers
				g_eventHandlers[queueEvent.command](&queueEvent);
			}
			else {
				ESP_LOGW("main", "Skipped unknown queue event '%d' in the main queue", queueEvent.command);
			}
		}
	}
}

void handleCanMessage(const QueueEvent_t* p_queueEvent)
{
	ESP_LOGD("main", "Received new can message");

	// Get the frame
	const twai_frame_t recFrame = p_queueEvent->canFrame;
	// Registration process
	if ((recFrame.header.id >> CAN_FRAME_HEADER_OFFSET) == CAN_MSG_REGISTRATION && recFrame.buffer_len >= 6) {
		// Send it to the Display Manager
		displayRegisterWithUUID(p_queueEvent->canFrame.buffer);
	}
}

void initOperationMode(const QueueEvent_t* p_queueEvent)
{
	// Are we already in the OPERATION mode but not yet initialized?
	if (getCurrentState() == STATE_OPERATION && g_operationModeInitialized == false) {
		bool initSucceeded = true;

		/*
		 * Start the Webinterface
		 */
		// initSucceeded &= startWebInterface(GET_FROM_CONFIG);

		// Start the reading of the speed and RPM sensor
		sensorManagerEnableSpeedISR();
		sensorManagerEnableRpmISR();

		// Finished the initialization
		g_operationModeInitialized = initSucceeded;
	}
}

/*
 *	Function Implementations
 */

void debugListAllSpiffsFiles()
{
	struct dirent* de;
	DIR* dr = opendir("/config");
	if (dr == NULL) {
		printf("Konnte Verzeichnis nicht öffnen: %s\n", "/config");
		return;
	}
	while ((de = readdir(dr)) != NULL) {
		printf("Gefundene Datei: %s\n", de->d_name);
	}
	closedir(dr);
}
