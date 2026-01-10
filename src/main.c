// Project includes
#include "FilesystemDriver.h"
#include "../include/UpdateManagers/DisplayCanUpdater.h"
#include "../include/WebInterface/WebInterface.h"
#include "DisplayManager.h"
#include "EventQueues.h"
#include "SensorManager.h"
#include "Version.h"
#include "can.h"
#include "can_messages.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include <sys/dirent.h>
#include <sys/stat.h>

/*
 *	Defines
 */
// Config file
#define WIFI_CONFIG_NAME "wifi_config.json"

/*
 *	Prototypes
 */
//! \brief Handles received CAN frame
//! \param p_queueEvent A pointer to the CAN frame
static void handleCanFrame(const QueueEvent_t* p_queueEvent);

//! \brief Enters the operating mode
static void enterOperatingMode();

//! \brief Handles a CAN driver crash
static void handleCanDriverCrash();

/*
 *	Private Variables
 */
uint8_t g_ownCanComId = 0x00;

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
	ESP_LOGI("main", "--- --- --- --- --- --- ---");
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
	canInitializeNode(GPIO_NUM_43, GPIO_NUM_2);
	canEnableNode();

	// Register the main queue to the CAN bus
	canRegisterRxCbQueue(&g_mainEventQueue);

	// Send a UUID Request
	QueueEvent_t initRequest;
	initRequest.command = START_REGISTRATION_PROCESS;
	xQueueSend(g_mainEventQueue, &initRequest, portMAX_DELAY);

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// The CAN driver crashed
			if (queueEvent.command == CAN_DRIVER_CRASHED) {
				handleCanDriverCrash();

				continue;
			}

			// We received a new CAN frame
			if (queueEvent.command == RECEIVED_NEW_CAN_FRAME) {
				handleCanFrame(&queueEvent);

				continue;
			}

			// Starting the CAN registration process
			if (queueEvent.command == START_REGISTRATION_PROCESS) {
				displayStartRegistrationProcess();

				continue;
			}

			// Enter the operation mode
			if (queueEvent.command == INIT_OPERATION_MODE) {
				enterOperatingMode();

				continue;
			}
		}
	}
}

void handleCanFrame(const QueueEvent_t* p_queueEvent)
{
	// Get the CAN frame
	const twai_frame_t rxFrame = p_queueEvent->canFrame;

	// Get the message id
	const uint8_t messageId = rxFrame.header.id >> CAN_MESSAGE_ID_OFFSET;

	// Get the sender com id
	uint8_t senderComId = rxFrame.header.id & 0x1FFFFF; // Zero top 8 bits

	// Registration process
	if (messageId == CAN_MSG_REGISTRATION && rxFrame.buffer_len >= 6) {
		// Pass it to the Display Manager
		senderComId = displayRegisterWithUUID(p_queueEvent->canFrame.buffer);

		/*
		 *	Request the firmware version
		 */
		// Create the CAN answer frame
		TwaiFrame_t frame;

		// Set the com id
		frame.buffer[0] = senderComId;

		// Initiate the frame
		canInitiateFrame(&frame, CAN_MSG_REQUEST_FIRMWARE_VERSION, 1);

		// Send the frame
		canQueueFrame(&frame);

		return;
	}

	// Answer to the firmware version request
	if (messageId == CAN_MSG_REQUEST_FIRMWARE_VERSION && rxFrame.buffer_len >= 4) {
		/*
		 *	Pass it to the Display Manager
		 */
		displaySetFirmwareVersion(senderComId, p_queueEvent->canFrame.buffer);

		/*
		 * Request the Commit Information
		 */
		// Create the CAN answer frame
		TwaiFrame_t frame;

		// Set the com id
		frame.buffer[0] = senderComId;

		// Initiate the frame
		canInitiateFrame(&frame, CAN_MSG_REQUEST_COMMIT_INFORMATION, 1);

		// Send the frame
		canQueueFrame(&frame);

		return;
	}

	// Answer to the commit information request
	if (messageId == CAN_MSG_REQUEST_COMMIT_INFORMATION && rxFrame.buffer_len >= 4) {
		/*
		 *	Pass it to the Display Manager
		 */
		displaySetCommitInformation(senderComId, p_queueEvent->canFrame.buffer);
		return;
	}
}

void enterOperatingMode()
{
	// Are we not yet initialized?
	if (g_operationModeInitialized) {
		return;
	}

	// Start the Webinterface
	// startWebInterface(GET_FROM_CONFIG);

	// Start the reading of all sensors
	sensorsStartReadingAllSensors();

	// Start sending the sensor data via CAN
	sensorsStartSendingSensorData();

	// Initialization completed
	g_operationModeInitialized = true;
}

void handleCanDriverCrash()
{
	if (canRecoverDriver() == ESP_OK) {
		ESP_LOGI("main", "Recovered CAN driver");

		return;
	}

	ESP_LOGE("main", "Couldn't recover CAN driver");
}
