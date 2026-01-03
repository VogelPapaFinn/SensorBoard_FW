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

// Intervals
#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

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
	[INIT_OPERATION_MODE]      = initOperationMode,
	[READ_SENSOR_DATA]         = readSensorData,
	[SENSOR_DATA_CHANGED]      = handleSensorDataChanged,
	[DISPLAY_STATI_CHANGED]    = handleDisplayStatiChanged
};
const uint8_t g_amountOfEventHandlers = sizeof(g_eventHandlers) / sizeof(EventHandlerFunction_t);

static bool g_operationModeInitialized = false;

static esp_timer_handle_t g_readSensorDataTimerHandle;
static const esp_timer_create_args_t g_readSensorDataTimerConf = {.callback = &readSensorDataISR,
                                                                  .name = "Read Sensor Data Timer"};


/*
 *	Interrupt Service Routines
 */
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
	filesystemInit();

	// Create the event queues
	createEventQueues();

	// Initialize the can node
	initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Initialize the Sensor Manager
	sensorManagerInit();

	// Initialize the Display Manager
	displayManagerInit();

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
			} else {
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

void readSensorData(const QueueEvent_t* p_queueEvent)
{
	sensorManagerUpdateFuelLevel();
	sensorManagerUpdateInternalTemperature();
	sensorManagerUpdateOilPressure();
	sensorManagerUpdateRPM();
	sensorManagerUpdateSpeed();
	sensorManagerUpdateSpeed();
}

void handleSensorDataChanged(const QueueEvent_t* p_queueEvent)
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

void handleDisplayStatiChanged(const QueueEvent_t* p_queueEvent)
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

