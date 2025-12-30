// Project includes
#include "DataCenter.h"
#include "FileManager.h"
#include "Global.h"
#include "SensorManager.h"
#include "WebInterface.h"
#include "can.h"
#include "can_messages.h"
#include "logger.h"
#include "statemachine.h"

// espidf includes
#include <esp_mac.h>
#include <esp_timer.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include "ConfigManager.h"
#include "../../esp-idf/components/json/cJSON/cJSON.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 *	Defines
 */
// CAN
#define CAN_SENDER_ID 0x00

// Intervals
#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

/*
 *	Prototypes
 */
void restartDisplay(const QUEUE_EVENT_T* queueEvent);
void handleCanMessage(const QUEUE_EVENT_T* queueEvent);
void requestUUIDs(const QUEUE_EVENT_T* queueEvent);
void initOperationMode(const QUEUE_EVENT_T* queueEvent);
void readSensorData(const QUEUE_EVENT_T* queueEvent);
void handleSensorDataChanged(const QUEUE_EVENT_T* queueEvent);
void handleDisplayStatiChanged(const QUEUE_EVENT_T* queueEvent);

void requestUUIDsISR(void* arg);
void readSensorDataISR(void* arg);

void sendUUIDRequest(void);

void IRAM_ATTR speedISR();
void IRAM_ATTR rpmISR();

void debugListAllSpiffsFiles();
void printDisplayConfigurationFile(const FILE* file);

/*
 *	Private Variables
 */
static esp_timer_handle_t uuidTimerHandle_;
static const esp_timer_create_args_t uuidTimerConf_ = {.callback = &requestUUIDsISR, .name = "Request HW UUIDs Timer"};
static uint8_t uuidRequestCounter_;

static bool operationModeInitialized_ = false;

static esp_timer_handle_t readSensorDataTimerHandle_;
static const esp_timer_create_args_t readSensorDataTimerConf_ = {.callback = &readSensorDataISR,
                                                                 .name = "Read Sensor Data Timer"};


/*
 *	Interrupt Service Routines
 */

//! \brief Connected to a Timer timeout to send UUID requests until we received all of them
void requestUUIDsISR(void* arg)
{
	// Create the event
	QUEUE_EVENT_T registerHwUUID;
	registerHwUUID.command = QUEUE_REQUEST_UUID;

	// Increase the timer
	uuidRequestCounter_++;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(mainEventQueue, &registerHwUUID, &xHigherPriorityTaskWoken);
}

//! \brief Connected to a Timer timeout to read the data from all sensor
void readSensorDataISR(void* arg)
{
	// Create the event
	QUEUE_EVENT_T readData;
	readData.command = QUEUE_READ_SENSOR_DATA;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(mainEventQueue, &readData, &xHigherPriorityTaskWoken);
}

/*
 *	Main functions
 */
void app_main(void)
{
	// Initialize the File Manager
	fileManagerInit();

	// Initialize the Config Manager
	configManagerInit();

	// Create the event queues
	createEventQueues();

	// Initialize the Data Center
	dataCenterInit();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Initialize the Sensor Manager
	sensorManagerInit();

	// Register the queue to the CAN bus
	registerCanRxCbQueue(&mainEventQueue);

	// Register the queue to the Data Center
	registerDataCenterCbQueue(&mainEventQueue);

	// Send a UUID Request
	QUEUE_EVENT_T initRequest;
	initRequest.command = QUEUE_REQUEST_UUID;
	xQueueSend(mainEventQueue, &initRequest, portMAX_DELAY);

	// Wait for new queue events
	QUEUE_EVENT_T queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Act depending on the event
			switch (queueEvent.command) {
				/*
				 *	Restart Display
				 */
				case QUEUE_RESTART_DISPLAY:
					restartDisplay(&queueEvent);

					break;

				/*
				 *	We received a CAN message
				 */
				case QUEUE_RECEIVED_NEW_CAN_MESSAGE:
					handleCanMessage(&queueEvent);

					break;


				/*
				 * Request the HW UUID of all devices
				 */
				case QUEUE_REQUEST_UUID:
					requestUUIDs(&queueEvent);

					break;

				/*
				 *	Initialization of the OPERATION mode
				 */
				case QUEUE_INIT_OPERATION_MODE:
					initOperationMode(&queueEvent);

					break;


				/*
				 *	Read the sensor data
				 */
				case QUEUE_READ_SENSOR_DATA:
					readSensorData(&queueEvent);

					break;

				/*
				 *	Send the sensor data
				 */
				case QUEUE_SENSOR_DATA_CHANGED:
					handleSensorDataChanged(&queueEvent);

					break;

				/*
				 *	Display Stati Changed
				 */
				case QUEUE_DISPLAY_STATI_CHANGED:
					handleDisplayStatiChanged(&queueEvent);

					break;

				/*
				 *	Fallback
				 */
				default:
					break;
			}
		}
	}
}

void restartDisplay(const QUEUE_EVENT_T* queueEvent)
{
	// Do we have parameters?
	if (queueEvent->parameterLength <= 0) {
		loggerDebug("Not enough parameters");

		return;
	}

	// Create the can frame answer
	twai_frame_t* restartFrame =
		generateCanFrame(CAN_MSG_DISPLAY_RESTART, CAN_SENDER_ID, queueEvent->parameter, queueEvent->parameterLength);

	// Send the frame
	queueCanBusMessage(restartFrame, true, true);
}

void handleCanMessage(const QUEUE_EVENT_T* queueEvent)
{
	loggerDebug("Received new can message");

	// Get the frame
	const twai_frame_t recFrame = queueEvent->canFrame;
	// Registration process
	if ((recFrame.header.id >> 21) == CAN_MSG_REGISTRATION && recFrame.buffer_len >= 6) {
		/*
		 *	Registration
		 */

		// Extract the UUID
		char* formattedUUID = malloc(sizeof(char) * 24);
		snprintf(formattedUUID, 24, "%d-%d-%d-%d-%d-%d", recFrame.buffer[5], recFrame.buffer[4], recFrame.buffer[3],
		         recFrame.buffer[2], recFrame.buffer[1], recFrame.buffer[0]);

		// Get the display configuration
		cJSON* displayConfiguration = getDisplayConfiguration();
		if (displayConfiguration == NULL) {
			loggerError("Couldn't handle display registration process because the display configuration isn't available!");
			return;
		}


		// Get the display configurations cJSON_Array
		cJSON* displayConfigurationsArray = cJSON_GetObjectItem(displayConfiguration, "displayConfigurations");

		// Check if there is a matching entry
		uint8_t matchedEntry = 255;
		for (uint8_t i = 0; i < (uint8_t)cJSON_GetArraySize(displayConfigurationsArray); i++) {
			// Get the entry
			cJSON* configuration = cJSON_GetArrayItem(displayConfigurationsArray, i);

			// Get the UUID
			cJSON* hwUUID = cJSON_GetObjectItem(configuration, "hwUUID");
			if (cJSON_IsString(hwUUID) && (hwUUID->valuestring != NULL)) {
				// Is it the same UUID?
				if (strcmp(hwUUID->valuestring, formattedUUID) == 0) {
					matchedEntry = i;
					break;
				}
			}
		}

		// Allocate the memory for the CAN frame
		uint8_t* buffer = malloc(sizeof(uint8_t) * 7);
		if (buffer == NULL) {
			free(formattedUUID);
			return;
		}

		// Insert the UUID
		buffer[0] = recFrame.buffer[0];
		buffer[1] = recFrame.buffer[1];
		buffer[2] = recFrame.buffer[2];
		buffer[3] = recFrame.buffer[3];
		buffer[4] = recFrame.buffer[4];
		buffer[5] = recFrame.buffer[5];
		buffer[6] = ++amountOfConnectedDisplays; // COM ID

		// Do we have a matching entry?
		if (matchedEntry != 255) {
			// Yes, get the entry
			const cJSON* configuration = cJSON_GetArrayItem(displayConfigurationsArray, matchedEntry);

			// Load the screen
			const cJSON* screen = cJSON_GetObjectItem(configuration, "screen");
			if (cJSON_IsString(screen) && (screen->valuestring != NULL)) {
				// Temperature screen
				if (strcmp(screen->valuestring, "temperature") == 0) {
					buffer[7] = 0;
				}
				// Speed screen
				else if (strcmp(screen->valuestring, "speed") == 0) {
					buffer[7] = 1;
				}
				// RPM screen
				else {
					buffer[7] = 2;
				}
			}
		}
		else {
			// Create the JSON object
			cJSON* configuration = cJSON_CreateObject();

			// Add the HW UUID
			cJSON_AddStringToObject(configuration, "hwUUID", formattedUUID);

			// Add the screen
			cJSON_AddStringToObject(configuration, "screen", "temperature");
			buffer[7] = 0;

			// Add it as an entry
			cJSON_AddItemToArray(displayConfigurationsArray, configuration);

			// Write the changes to the file
			writeDisplayConfigurationToFile();
		}

		/*
		 *	Track display stati
		 */
		// Get all displays
		Display_t* displays = getDisplayStatiObjects();
		Display_t* display = NULL;

		// Check if the one that connected is already being tracked
		for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
			// Does the uuid match?
			if (displays[i].uuid != NULL && strcmp(displays[i].uuid, formattedUUID) == 0) {
				display = &displays[i];
				break;
			}
			// Nope and all following entries are default ones, so take this one
			else if (displays[i].uuid == NULL) {
				display = &displays[i];
				break;
			}
		}

		// Set its values
		if (display != NULL) {
			display->connected = true;
			display->uuid = formattedUUID;
			display->comId = buffer[6];
			display->screen = buffer[7];
			display->wifiStatus = malloc(strlen("Turned Off") + 1);
			strcpy(display->wifiStatus, "Turned Off");

			// Notify everybody that the display stati changed
			broadcastDisplayStatiChanged();
		}

		/*
		 *	Sending CAN frame
		*/
		// Create the can frame answer
		twai_frame_t* frame = generateCanFrame(CAN_MSG_COMID_ASSIGNATION, CAN_SENDER_ID, buffer, 8);

		// Send the frame
		queueCanBusMessage(frame, true, true);

		// Logging
		loggerInfo("Sending ID '%d' and screen '%d' to UUID '%d-%d-%d-%d-%d-%d'", buffer[6], buffer[7], buffer[0],
		           buffer[1],
		           buffer[2], buffer[3], buffer[4], buffer[5]);

		/*
		 *	Cleanup
		 */
		free(formattedUUID);
	}
}

void requestUUIDs(const QUEUE_EVENT_T* queueEvent)
{
	// Did we receive all HW UUIDs?
	bool allUUIDsReceived = true;
	Display_t* displays = getDisplayStatiObjects();
	for (int i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		if (!(displays + i)->connected) {
			// No
			allUUIDsReceived = false;
			break;
		}
	}

	// Do we need to re-request?
	if (!allUUIDsReceived) {
		// Send the request
		sendUUIDRequest();

		// And restart the timer
		const uint64_t timeout = uuidRequestCounter_ >= 10 ? 2000 * 1000 : 200 * 1000;
		if (uuidTimerHandle_ == NULL) {
			esp_timer_create(&uuidTimerConf_, &uuidTimerHandle_);
		}
		if (!esp_timer_is_active(uuidTimerHandle_)) {
			esp_timer_start_once(uuidTimerHandle_, timeout);
		}

		// Should we switch to the operation state?
		// if (uuidRequestCounter_ >= 10 && currState != STATE_OPERATION) {
		//	setCurrentState(STATE_OPERATION);
		//
		//	// Log it
		//	loggerInfo("Not all displays registered themselves within 2 Seconds");
		//
		//	// Queue the init of the operation mode
		//	QUEUE_EVENT_T rq;
		//	rq.command = QUEUE_INIT_OPERATION_MODE;
		//	xQueueSend(mainEventQueue, &rq, pdMS_TO_TICKS(50));
		//}
	}
	else {
		// Logging
		loggerInfo("All displays registered themselves");

		// Delete the timer
		esp_timer_delete(uuidTimerHandle_);
		uuidTimerHandle_ = NULL;

		// And then enter the OPERATION mode
		setCurrentState(STATE_OPERATION);

		// Queue the init of the operation mode
		QUEUE_EVENT_T rq;
		rq.command = QUEUE_INIT_OPERATION_MODE;
		xQueueSend(mainEventQueue, &rq, portMAX_DELAY);
	}
}

void initOperationMode(const QUEUE_EVENT_T* queueEvent)
{
	// Are we already in the OPERATION mode but not yet initialized?
	if (getCurrentState() == STATE_OPERATION && operationModeInitialized_ == false) {
		bool initSucceeded = true;

		/*
		 * Start the Webinterface
		 */
		initSucceeded &= startWebInterface(GET_FROM_CONFIG);

		/*
		 *	Start reading the sensor data
		 */
		if (readSensorDataTimerHandle_ == NULL) {
			initSucceeded &= esp_timer_create(&readSensorDataTimerConf_, &readSensorDataTimerHandle_);
		}
		if (!esp_timer_is_active(readSensorDataTimerHandle_)) {
			initSucceeded &=
				esp_timer_start_periodic(readSensorDataTimerHandle_, READ_SENSOR_DATA_INTERVAL_MS * 1000);
		}

		// Start the reading of the speed and RPM sensor
		sensorManagerEnableSpeedISR();
		sensorManagerEnableRpmISR();

		// Finished the initialization
		operationModeInitialized_ = initSucceeded;
	}
}

void readSensorData(const QUEUE_EVENT_T* queueEvent)
{
	bool dataUpdated = false;
	dataUpdated |= sensorManagerUpdateFuelLevel();
	dataUpdated |= sensorManagerUpdateInternalTemperature();
	dataUpdated |= sensorManagerUpdateOilPressure();
	dataUpdated |= sensorManagerUpdateRPM();
	dataUpdated |= sensorManagerUpdateSpeed();
	dataUpdated |= sensorManagerUpdateSpeed();

	// Did something change?
	if (dataUpdated) {
		broadcastSensorDataChanged();
	}
}

void handleSensorDataChanged(const QUEUE_EVENT_T* queueEvent)
{
	// Are we successfully initialize?
	if (!operationModeInitialized_) {
		return;
	}

	// Create the buffer for the answer CAN frame
	uint8_t* buffer = malloc(sizeof(uint8_t) * 8);
	if (buffer == NULL) {
		return;
	}
	buffer[0] = vehicleSpeed;
	buffer[1] = vehicleRPM >> 8;
	buffer[2] = (uint8_t)vehicleRPM;
	buffer[3] = fuelLevelInPercent;
	buffer[4] = waterTemp;
	buffer[5] = oilPressure;
	buffer[6] = leftIndicator;
	buffer[7] = rightIndicator;

	// Create the CAN answer frame
	twai_frame_t* sensorDataFrame = generateCanFrame(CAN_MSG_SENSOR_DATA, CAN_SENDER_ID, buffer, 8);

	// Send the frame
	queueCanBusMessage(sensorDataFrame, true, true);
}

void handleDisplayStatiChanged(const QUEUE_EVENT_T* queueEvent)
{
	// Get all displays
	const char* jsonOutput = getAllDisplayStatiAsJSON();

	// Send the data to the webserver
	if (jsonOutput != NULL) {
		webinterfaceSendData(jsonOutput);
	}
}

/*
 *	Function Implementations
 */

void sendUUIDRequest(void)
{
	// Create the can frame
	twai_frame_t* frame = generateCanFrame(CAN_MSG_REGISTRATION, CAN_SENDER_ID, NULL, 0);

	// Send the frame
	queueCanBusMessage(frame, true, false);

	// Debug Logging
	// loggerDebug("Sent HW UUID Request");
}

void debugListAllSpiffsFiles()
{
	struct dirent *de;
	DIR *dr = opendir("/config");
	if (dr == NULL) {
		printf("Konnte Verzeichnis nicht öffnen: %s\n", "/config");
		return;
	}
	while ((de = readdir(dr)) != NULL) {
		printf("Gefundene Datei: %s\n", de->d_name);
	}
	closedir(dr);
}

void printDisplayConfigurationFile(const FILE* file)
{
	loggerDebug("--- Content of 'displays_config.json' ---");
	char line[256];
	while (fgets(line, sizeof(line), (FILE*)file) != NULL) {
		printf("%s", line);
	}
	printf("\n");
	loggerDebug("--- End of 'displays_config.json' ---");
}
