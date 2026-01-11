#include "Managers/OperationManager.h"

// Project includes
#include "DisplayCenter.h"
#include "Managers/CanUpdateManager.h"
#include "Managers/RegistrationManager.h"
#include "SensorCenter.h"
#include "can.h"

// espidf includes
#include <esp_log.h>
#include <esp_timer.h>

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Private defines
 */
#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

/*
 *	Prototypes
*/
static void readSensorDataISR(void* p_arg);

static void sendSensorDataISR(void* p_arg);

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_canTaskHandle;

//! \brief Task handle of the event task
static TaskHandle_t g_eventTaskHandle;

//! \brief Handle of timer which is used to periodically read all available sensors
static esp_timer_handle_t g_readSensorDataTimerHandle;

//! \brief The configuration of the timer which is used to periodically read all available sensors
static const esp_timer_create_args_t g_readSensorDataTimerConf = {.callback = &readSensorDataISR,
                                                                  .name = "Read Sensor Data Timer"};

//! \brief Handle of timer which is used to periodically send all available sensor data via CAN
static esp_timer_handle_t g_sendSensorDataTimerHandle;

//! \brief The configuration of the timer which is used to periodically send all available sensor data via CAN
static const esp_timer_create_args_t g_sendSensorDataTimerConf = {.callback = &sendSensorDataISR,
                                                                  .name = "Send Sensor Data Timer"};


/*
 *	Tasks & ISRs
 */
//! \brief Task used to receive and handle CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param)
{
	// Wait for new queue events
	twai_frame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_operationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		/*
		 *	Preparations
		 */
		// Get the frame id
		const uint8_t frameId = rxFrame.header.id >> CAN_FRAME_ID_OFFSET;

		// Get the sender com id
		const uint8_t senderId = rxFrame.header.id & 0x1FFFFF; // Zero top 8 bits

		/*
		 *	Com id specific frames
		 */
		// Skip if we were not meant
		if (rxFrame.header.dlc == 0 || (rxFrame.header.dlc > 0 && *rxFrame.buffer != g_ownCanComId)) {
			continue;
		}

		// A frame containing the firmware version of a display
		if (frameId == CAN_MSG_REQUEST_FIRMWARE_VERSION) {
			/*
			 *	Pass it to the Display Manager
			 */
			displaySetFirmwareVersion(senderId, rxFrame.buffer);

			/*
			 * Request the Commit Information
			 */
			// Create the CAN answer frame
			TwaiFrame_t frame;

			// Set the com id
			frame.buffer[0] = senderId;

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_REQUEST_COMMIT_INFORMATION, 1);

			// Send the frame
			canQueueFrame(&frame);

			return;
		}

		// Answer to the commit information request
		if (frameId == CAN_MSG_REQUEST_COMMIT_INFORMATION && rxFrame.buffer_len >= 4) {
			/*
			 *	Pass it to the Display Manager
			 */
			displaySetCommitInformation(senderId, rxFrame.buffer);
			return;
		}

	}
}

//! \brief Task used to receive and handle events
//! \param p_param Unused parameters
static void eventTask(void* p_param)
{
	// Wait for new queue events
	QueueEvent_t event;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_operationManagerEventQueue, &event, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Read new sensor data?
		if (event.command == READ_SENSOR_DATA) {
			sensorsReadAll();
			continue;
		}

		// Send new sensor data?
		if (event.command == SEND_SENSOR_DATA) {
			sensorsSendAll();
			continue;
		}
	}
}

static void readSensorDataISR(void* p_arg)
{
	QueueEvent_t event;
	event.command = READ_SENSOR_DATA;
	xQueueSend(g_operationManagerEventQueue, &event, portMAX_DELAY);
}

static void sendSensorDataISR(void* p_arg)
{
	QueueEvent_t event;
	event.command = SEND_SENSOR_DATA;
	xQueueSend(g_operationManagerEventQueue, &event, portMAX_DELAY);
}

/*
 *	Public function implementations
 */
bool operationManagerInit()
{
	// Register to the CAN rx cb
	if (!canRegisterRxCbQueue(&g_operationManagerCanQueue)) {
		ESP_LOGE("OperationManager", "Couldn't register rx cb queue");

		return false;
	}

	// Start the can task
	if (xTaskCreate(canTask, "OperationManagerCanTask", 2048 * 4, NULL, 0, &g_canTaskHandle) != pdPASS) {
		ESP_LOGE("OperationManager", "Couldn't create CAN task!");

		return false;
	}

	// Start the event task
	if (xTaskCreate(eventTask, "OperationManagerEventTask", 2048 * 4, NULL, 0, &g_eventTaskHandle) != pdPASS) {
		ESP_LOGE("OperationManager", "Couldn't create event task!");

		return false;
	}

	// Destroy the registration manager
	registrationManagerDestroy();

	// Start the Webinterface
	// startWebInterface(GET_FROM_CONFIG);

	// Create the read and send sensors timers
	if (esp_timer_create(&g_readSensorDataTimerConf, &g_readSensorDataTimerHandle) != ESP_OK) {
		ESP_LOGE("OperationManager", "Couldn't create read sensor timer");
	}
	if (esp_timer_create(&g_sendSensorDataTimerConf, &g_sendSensorDataTimerHandle) != ESP_OK) {
		ESP_LOGE("OperationManager", "Couldn't create send sensor timer");
	}

	// Start the read and send sensors timers
	operationManagerStartReadingSensors();
	operationManagerStartSendingSensors();

	// Initiate the CAN Update Manager
	canUpdateManagerInit();

	return true;
}

void operationManagerStartReadingSensors()
{
	if (esp_timer_start_periodic(g_readSensorDataTimerHandle, (uint64_t)(READ_SENSOR_DATA_INTERVAL_MS * 1000)) != ESP_OK) { // NOLINT
		ESP_LOGE("OperationManager", "Couldn't start read sensor timer");
	}
}

void operationManagerStopReadingSensors()
{
	esp_timer_stop(g_readSensorDataTimerHandle);
}

void operationManagerStartSendingSensors()
{
	if (esp_timer_start_periodic(g_sendSensorDataTimerHandle, (uint64_t)(SEND_SENSOR_DATA_INTERVAL_MS * 1000)) != ESP_OK) { // NOLINT
		ESP_LOGE("OperationManager", "Couldn't start send sensor timer");
	}
}

void operationManagerStopSendingSensors()
{
	esp_timer_stop(g_sendSensorDataTimerHandle);
}

void operationManagerDestroy()
{
	// Stop and delete the timers
	esp_timer_stop(g_readSensorDataTimerHandle);
	esp_timer_stop(g_sendSensorDataTimerHandle);
	esp_timer_delete(g_readSensorDataTimerHandle);
	esp_timer_delete(g_sendSensorDataTimerHandle);

	// Unregister from the CAN rx cb
	canUnregisterRxCbQueue(&g_operationManagerCanQueue);

	// Destroy the CAN task
	vTaskDelete(g_canTaskHandle);

	// Destroy the event task
	vTaskDelete(g_eventTaskHandle);
}
