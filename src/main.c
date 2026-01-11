// Project includes
#include "FilesystemDriver.h"
#include "DisplayCenter.h"
#include "EventQueues.h"
#include "Managers/RegistrationManager.h"
#include "SensorCenter.h"
#include "Version.h"
#include "can.h"

// espidf includes
#include "esp_log.h"
#include <sys/dirent.h>
#include <sys/stat.h>

/*
 *	Private Variables
 */
uint8_t g_ownCanComId = 0x00;

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
	/*
	 *	Initial Logging
	 */
	ESP_LOGI("main", "--- --- --- --- --- --- ---");
	ESP_LOGI("main", "Firmware Version: %s", VERSION_FULL);

	/*
	 *	Initialization of Drivers etc.
	 */
	// Initialize the File Manager
	filesystemInit();

	// Create the event queues
	createEventQueues();

	// Initialize the Sensor Manager
	sensorManagerInit();

	// Initialize the Display Manager
	displayCenterInit();

	// Initialize the can node
	canInitializeNode(GPIO_NUM_43, GPIO_NUM_2);
	canEnableNode();

	/*
	 *	Other preparations
	 */
	// Register the main queue to the CAN bus
	canRegisterRxCbQueue(&g_mainEventQueue);

	/*
	 *	Initialization of the registration manager
	 */
	registrationManagerInit();

	// Wait for new queue events
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_mainEventQueue, &queueEvent, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// The CAN driver crashed
		if (queueEvent.command == CAN_DRIVER_CRASHED) {
			if (canRecoverDriver() == ESP_OK) {
				ESP_LOGI("main", "Recovered CAN driver");

				return;
			}

			ESP_LOGE("main", "Couldn't recover CAN driver");

			continue;
		}

		// Restart a display
		if (queueEvent.command == RESTART_DISPLAY) {
			if (queueEvent.parameterLength == 0) {
				continue;
			}

			// Get the display com id
			const uint8_t comId = *(uint8_t*)queueEvent.parameter;

			/*
			 *	Send CAN frame
			 */
			// Create the CAN answer frame
			TwaiFrame_t frame;

			// Set the com id
			frame.buffer[0] = comId;

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_DISPLAY_RESTART, 1);

			// Send the frame
			canQueueFrame(&frame);

			continue;
		}
	}
}
