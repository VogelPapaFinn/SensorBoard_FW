// Project includes
#include "FilesystemDriver.h"
#include "EventQueues.h"
#include "SensorManager.h"
#include "../include/WebInterface/WebInterface.h"
#include "can.h"
#include "can_messages.h"
#include "statemachine.h"
#include "DisplayManager.h"
#include "Version.h"

// espidf includes
#include <esp_mac.h>
#include <esp_timer.h>
#include <sys/dirent.h>
#include <sys/stat.h>
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
static void handleCanMessage(const QueueEvent_t* p_queueEvent);
static void enterOperatingMode(const QueueEvent_t* p_queueEvent);
static void handleCanDriverCrash(const QueueEvent_t* p_queueEvent);

void debugListAllSpiffsFiles();

/*
 *	Private typedefs
 */
//! \brief A typedef enum indicating the state we are in
typedef enum
{
	STATE_INIT,
	STATE_OPERATING,
} State_t;

typedef void (*EventHandlerFunction_t)(const QueueEvent_t* p_queueEvent);

/*
 *	Private Variables
 */
uint8_t g_ownCanComId = 0x00;
static const EventHandlerFunction_t g_eventHandlers[] = {
	[CAN_DRIVER_CRASHED] = handleCanDriverCrash,
	[RECEIVED_NEW_CAN_MESSAGE] = handleCanMessage,
	[REQUEST_UUID] = displayStartRegistrationProcess,
	[INIT_OPERATION_MODE] = enterOperatingMode,
};
const uint8_t g_amountOfEventHandlers = sizeof(g_eventHandlers) / sizeof(EventHandlerFunction_t);

static bool g_operationModeInitialized = false;

/*
 *	Helper function implementations
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

/*
 *	Main functions
 */
void app_main(void) // NOLINT - Deactivate clang-tiny for this line
{
	ESP_LOGI("main", "Firmware Version: %s", VERSION_FULL);

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
	// Get the frame
	const twai_frame_t recFrame = p_queueEvent->canFrame;

	// Get the message id
	const uint8_t messageId = recFrame.header.id >> CAN_FRAME_HEADER_OFFSET;

	// Get the sender com id
	const uint32_t senderComId = (uint8_t)recFrame.header.id;

	ESP_LOGI("main", "Received new can message %d from sender %d", messageId, senderComId);

	// Registration process
	if (messageId == CAN_MSG_REGISTRATION && recFrame.buffer_len >= 6) {
		// Send it to the Display Manager
		displayRegisterWithUUID(p_queueEvent->canFrame.buffer);

		/*
		 * Request the firmware version
		 */
		// Create the buffer for the request CAN frame
		uint8_t* buffer = malloc(sizeof(uint8_t) * 1);
		if (buffer == NULL) {
			return;
		}
		buffer[0] = senderComId;

		// Create the CAN answer frame
		twai_frame_t* requestFrame = generateCanFrame(CAN_MSG_REQUEST_FIRMWARE_VERSION, g_ownCanComId, &buffer, 1);

		// Send the frame
		queueCanBusMessage(requestFrame, true, true);

		return;
	}

	// Answer to the firmware version request
	if (messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION && recFrame.buffer_len >= 4) {
		/*
		 *	Pass it to the Display Manager
		 */
		displaySetFirmwareVersion(senderComId, p_queueEvent->canFrame.buffer);

		/*
		 * Request the Commit Information
		 */
		// Create the buffer for the request CAN frame
		uint8_t* buffer = malloc(sizeof(uint8_t) * 1);
		if (buffer == NULL) {
			return;
		}
		buffer[0] = senderComId;

		// Create the CAN answer frame
		twai_frame_t* requestFrame = generateCanFrame(CAN_MSG_REQUEST_COMMIT_INFORMATION, g_ownCanComId, &buffer, 1);

		// Send the frame
		queueCanBusMessage(requestFrame, true, true);

		return;
	}

	// Answer to the commit information request
	if (messageId == CAN_MSG_REQUEST_COMMIT_INFORMATION && recFrame.buffer_len >= 4) {
		/*
		 *	Pass it to the Display Manager
		 */
		displaySetCommitInformation(senderComId, p_queueEvent->canFrame.buffer);

		return;
	}

	ESP_LOGW("main", "Received unknown can message with command id %d", (recFrame.header.id >> CAN_FRAME_HEADER_OFFSET));
}

void enterOperatingMode(const QueueEvent_t* p_queueEvent)
{
	// Are we not yet initialized?
	if (g_operationModeInitialized == false) {
		// Enter the operating mode
		setCurrentState(STATE_OPERATING);

		// Start the Webinterface
		// startWebInterface(GET_FROM_CONFIG);

		// Start the reading of all sensors
		sensorsStartReadingAllSensors();

		// Start sending the sensor data via can
		sensorsStartSendingSensorData();

		// Finished the initialization
		g_operationModeInitialized = true;
	}
}

void handleCanDriverCrash(const QueueEvent_t* p_queueEvent)
{
	if (recoverCanDriver() == ESP_OK) {
		ESP_LOGI("main", "Recovered CAN driver");
	} else {
		ESP_LOGE("main", "Couldn't recover CAN driver");
	}
}
