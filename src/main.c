// Project includes
#include "FileManager.h"
#include "QueueSystem.h"
#include "SensorManager.h"
#include "../include/WebInterface/WebInterface.h"
#include "can.h"
#include "can_messages.h"
#include "statemachine.h"
#include "ConfigManager.h"

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
// CAN
#define CAN_SENDER_ID 0x00
#define AMOUNT_OF_DISPLAYS 1

// Intervals
#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

/*
 *	Prototypes
 */
void restartDisplay(const QueueEvent_t* p_queueEvent);
void handleCanMessage(const QueueEvent_t* p_queueEvent);
void requestUUIDs(const QueueEvent_t* p_queueEvent);
void initOperationMode(const QueueEvent_t* p_queueEvent);
void readSensorData(const QueueEvent_t* p_queueEvent);
void handleSensorDataChanged(const QueueEvent_t* p_queueEvent);
void handleDisplayStatiChanged(const QueueEvent_t* p_queueEvent);

void requestUUIDsISR(void* p_arg);
void readSensorDataISR(void* p_arg);

void sendUUIDRequest(void);

void IRAM_ATTR speedISR();
void IRAM_ATTR rpmISR();

void debugListAllSpiffsFiles();
void printDisplayConfigurationFile(const FILE* p_file);

/*
 *	Private Variables
 */
static esp_timer_handle_t g_uuidTimerHandle;
static const esp_timer_create_args_t g_uuidTimerConf = {.callback = &requestUUIDsISR, .name = "Request HW UUIDs Timer"};
static uint8_t g_uuidRequestCounter;

static bool g_operationModeInitialized = false;

static esp_timer_handle_t g_readSensorDataTimerHandle;
static const esp_timer_create_args_t g_readSensorDataTimerConf = {.callback = &readSensorDataISR,
                                                                  .name = "Read Sensor Data Timer"};

//! \brief Amount of connected displays. Used for assigning the COM IDs
static uint8_t g_amountOfConnectedDisplays;


/*
 *	Interrupt Service Routines
 */

//! \brief Connected to a Timer timeout to send UUID requests until we received all of them
void requestUUIDsISR(void* p_arg)
{
	// Create the event
	QueueEvent_t registerHwUUID;
	registerHwUUID.command = REQUEST_UUID;

	// Increase the timer
	g_uuidRequestCounter++;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(g_mainEventQueue, &registerHwUUID, &xHigherPriorityTaskWoken);
}

//! \brief Connected to a Timer timeout to read the data from all sensor
void readSensorDataISR(void* p_arg)
{
	// Create the event
	QueueEvent_t readData;
	readData.command = READ_SENSOR_DATA;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(g_mainEventQueue, &readData, &xHigherPriorityTaskWoken);
}

/*
 *	Main functions
 */
void app_main(void) // NOLINT - Deactivate clang-tiny for this line
{
	// Initialize the File Manager
	fileManagerInit();

	// Initialize the Config Manager
	configManagerInit();

	// Create the event queues
	createEventQueues();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Initialize the Sensor Manager
	sensorManagerInit();

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
			switch (queueEvent.command) {
				/*
				 *	Restart Display
				 */
				case RESTART_DISPLAY:
					restartDisplay(&queueEvent);

					break;

				/*
				 *	We received a CAN message
				 */
				case RECEIVED_NEW_CAN_MESSAGE:
					handleCanMessage(&queueEvent);

					break;


				/*
				 * Request the HW UUID of all devices
				 */
				case REQUEST_UUID:
					requestUUIDs(&queueEvent);

					break;

				/*
				 *	Initialization of the OPERATION mode
				 */
				case INIT_OPERATION_MODE:
					initOperationMode(&queueEvent);

					break;


				/*
				 *	Read the sensor data
				 */
				case READ_SENSOR_DATA:
					readSensorData(&queueEvent);

					break;

				/*
				 *	Send the sensor data
				 */
				case SENSOR_DATA_CHANGED:
					handleSensorDataChanged(&queueEvent);

					break;

				/*
				 *	Display Stati Changed
				 */
				case DISPLAY_STATI_CHANGED:
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

void restartDisplay(const QueueEvent_t* p_queueEvent)
{
	// Do we have parameters?
	if (p_queueEvent->parameterLength <= 0) {
		ESP_LOGD("main", "Not enough parameters");

		return;
	}

	// Create the can frame answer
	twai_frame_t* restartFrame =
		generateCanFrame(CAN_MSG_DISPLAY_RESTART, CAN_SENDER_ID, p_queueEvent->parameter,
		                 p_queueEvent->parameterLength);

	// Send the frame
	queueCanBusMessage(restartFrame, true, true);
}

void handleCanMessage(const QueueEvent_t* p_queueEvent)
{
	ESP_LOGD("main", "Received new can message");

	// Get the frame
	const twai_frame_t recFrame = p_queueEvent->canFrame;
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
			ESP_LOGE("main",
			         "Couldn't handle display registration process because the display configuration isn't available!");
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
		buffer[6] = ++g_amountOfConnectedDisplays; // COM ID

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
		 *	Sending CAN frame
		*/
		// Create the can frame answer
		twai_frame_t* frame = generateCanFrame(CAN_MSG_COMID_ASSIGNATION, CAN_SENDER_ID, buffer, 8);

		// Send the frame
		queueCanBusMessage(frame, true, true);

		// Logging
		ESP_LOGI("main", "Sending ID '%d' and screen '%d' to UUID '%d-%d-%d-%d-%d-%d'", buffer[6], buffer[7], buffer[0],
		         buffer[1],
		         buffer[2], buffer[3], buffer[4], buffer[5]);

		/*
		 *	Cleanup
		 */
		free(formattedUUID);
	}
}

void requestUUIDs(const QueueEvent_t* p_queueEvent)
{
	// Do we need to re-request?
	if (g_amountOfConnectedDisplays < AMOUNT_OF_DISPLAYS) {
		// Send the request
		sendUUIDRequest();

		// And restart the timer
		const uint64_t timeout = g_uuidRequestCounter >= 10 ? 2000 * 1000 : 200 * 1000;
		if (g_uuidTimerHandle == NULL) {
			esp_timer_create(&g_uuidTimerConf, &g_uuidTimerHandle);
		}
		if (!esp_timer_is_active(g_uuidTimerHandle)) {
			esp_timer_start_once(g_uuidTimerHandle, timeout);
		}
	}
	else {
		// Logging
		ESP_LOGI("main", "All displays registered themselves");

		// Delete the timer
		esp_timer_delete(g_uuidTimerHandle);
		g_uuidTimerHandle = NULL;

		// And then enter the OPERATION mode
		setCurrentState(STATE_OPERATION);

		// Queue the init of the operation mode
		QueueEvent_t rq;
		rq.command = INIT_OPERATION_MODE;
		xQueueSend(g_mainEventQueue, &rq, portMAX_DELAY);
	}
}

void initOperationMode(const QueueEvent_t* queueEvent)
{
	// Are we already in the OPERATION mode but not yet initialized?
	if (getCurrentState() == STATE_OPERATION && g_operationModeInitialized == false) {
		bool initSucceeded = true;

		/*
		 * Start the Webinterface
		 */
		// initSucceeded &= startWebInterface(GET_FROM_CONFIG);

		/*
		 *	Start reading the sensor data
		 */
		if (g_readSensorDataTimerHandle == NULL) {
			initSucceeded &= esp_timer_create(&g_readSensorDataTimerConf, &g_readSensorDataTimerHandle);
		}
		if (!esp_timer_is_active(g_readSensorDataTimerHandle)) {
			initSucceeded &=
				esp_timer_start_periodic(g_readSensorDataTimerHandle, READ_SENSOR_DATA_INTERVAL_MS * 1000);
		}

		// Start the reading of the speed and RPM sensor
		test_function_snake_case();
		sensorManagerEnableRpmISR();

		// Finished the initialization
		g_operationModeInitialized = initSucceeded;
	}
}

void readSensorData(const QueueEvent_t* queueEvent)
{
	sensorManagerUpdateFuelLevel();
	sensorManagerUpdateInternalTemperature();
	sensorManagerUpdateOilPressure();
	sensorManagerUpdateRPM();
	sensorManagerUpdateSpeed();
	sensorManagerUpdateSpeed();
}

void handleSensorDataChanged(const QueueEvent_t* queueEvent)
{
	// Are we successfully initialize?
	if (!g_operationModeInitialized) {
		return;
	}

	// Create the buffer for the answer CAN frame
	// uint8_t* buffer = malloc(sizeof(uint8_t) * 8);
	// if (buffer == NULL) {
	// 	return;
	// }
	// buffer[0] = g_vehicleSpeed;
	// buffer[1] = g_vehicleRPM >> 8;
	// buffer[2] = (uint8_t)g_vehicleRPM;
	// buffer[3] = g_fuelLevelInPercent;
	// buffer[4] = g_waterTemp;
	// buffer[5] = g_oilPressure;
	// buffer[6] = g_leftIndicator;
	// buffer[7] = g_rightIndicator;

	// Create the CAN answer frame
	// twai_frame_t* sensorDataFrame = generateCanFrame(CAN_MSG_SENSOR_DATA, CAN_SENDER_ID, buffer, 8);

	// Send the frame
	// queueCanBusMessage(sensorDataFrame, true, true);
}

void handleDisplayStatiChanged(const QueueEvent_t* queueEvent)
{
	// // Get all displays
	// const char* jsonOutput = getAllDisplayStatiAsJSON();
	//
	// // Send the data to the webserver
	// if (jsonOutput != NULL) {
	// 	webinterfaceSendData();
	// }
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
	// ESP_LOGD("main", "Sent HW UUID Request");
}

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

void printDisplayConfigurationFile(const FILE* file)
{
	ESP_LOGD("main", "--- Content of 'displays_config.json' ---");
	char line[256];
	while (fgets(line, sizeof(line), (FILE*)file) != NULL) {
		printf("%s", line);
	}
	printf("\n");
	ESP_LOGD("main", "--- End of 'displays_config.json' ---");
}
