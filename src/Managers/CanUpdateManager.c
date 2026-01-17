#include "Managers/CanUpdateManager.h"

// Project includes
#include "Drivers/FilesystemDriver.h"
#include "can.h"

// C includes
#include <dirent.h>
#include <string.h>

// espidf includes
#include "esp_log.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

/*
 *	Private defines
 */
#define UPDATE_BLOCK_SIZE_B 7
#define UPDATE_SDCARD_FOLDER "updates"
#define UPDATE_FILE_NAME_PATTERN "Update_Display_%d.%d.%d-%255s" // e.g. "Update_2.0.1-bfe634f"
#define UPDATE_FILE_MAX_NAME_LENGTH 256

/*
 *	Prototypes
*/
//! \brief FreeRTOS task which handles all CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param);

//! \brief FreeRTOS task which handles all events in the event queue
//! \param p_param Unused parameters
static void eventTask(void* p_param);

//! \brief Handles the preparations of the update
//! \param p_queueEvent A pointer to the queue event
static void prepareUpdate(const QueueEvent_t* p_queueEvent);

//! \brief Handles the transmitting of the update
//! \param p_queueEvent A pointer to the queue event
static void transmitUpdate(const QueueEvent_t* p_queueEvent);

//! \brief Handles the execution of the update
//! \param p_queueEvent A pointer to the queue event
static void executeUpdate(const QueueEvent_t* p_queueEvent);

//! \brief Loads the size of the update file
static void loadUpdateFileSize();

/*
 *	Private variables
 */
//! \brief Bool indicating if an update file was found
static bool g_updateAvailable = false;

//! \brief The update size of the update file in bytes
static uint32_t g_fileSizeB = 0;

//! \brief The name of the update file
static char g_fileName[UPDATE_FILE_MAX_NAME_LENGTH] = {'\0'};

//! \brief Stream to read from the update file
static FILE* g_file = NULL;

//! \brief Task handle of the can task
static TaskHandle_t g_canTaskHandle;

//! \brief Task handle of the event task
static TaskHandle_t g_eventTaskHandle;

//! \brief Amount of total bytes transmitted
static uint32_t g_bytesTransmitted = 0;

/*
 *	Private tasks
 */
static void canTask(void* p_param)
{
	if (!g_updateAvailable) {
		vTaskDelete(NULL);
	}

	// Wait for new queue events
	TwaiFrame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_operationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Get the message id
		const uint8_t messageId = rxFrame.espidfFrame.header.id >> CAN_FRAME_ID_OFFSET;

		// Get the sender com id
		const uint32_t senderId = (uint8_t)rxFrame.espidfFrame.header.id;

		// Is it of the display we update?
		if (senderId != *(uint8_t*)p_param) {
			continue;
		}

		// Acknowledge of Initialization of Update
		if (messageId == CAN_MSG_PREPARE_UPDATE) {
			// Queue the event
			QueueEvent_t event;
			event.command = TRANSMIT_UPDATE;
			event.parameter = (void*)&senderId;
			event.parameterLength = sizeof(uint8_t);
			xQueueSend(g_canUpdateManagerEventQueue, &event, portMAX_DELAY);
			continue;
		}

		// Answer to the last transmitted update file block
		if (messageId == CAN_MSG_TRANSMIT_UPDATE_FILE) {
			// Was it the last block of the file?
			if (g_bytesTransmitted >= g_fileSizeB) {
				// Queue the execution of the update
				QueueEvent_t event;
				event.command = EXECUTE_UPDATE;
				event.parameter = (void*)&senderId;
				event.parameterLength = sizeof(uint8_t);
				xQueueSend(g_canUpdateManagerEventQueue, &event, portMAX_DELAY);

				continue;
			}

			// Queue the transmission of the next block
			QueueEvent_t event;
			event.command = TRANSMIT_UPDATE;
			event.parameter = (void*)&senderId;
			event.parameterLength = sizeof(uint8_t);
			xQueueSend(g_canUpdateManagerEventQueue, &event, portMAX_DELAY);

			continue;
		}

		// Acknowledge of the update execution
		if (messageId == CAN_MSG_EXECUTE_UPDATE) {
			// Unregister the update queue from the CAN
			canUnregisterRxCbQueue(&g_canUpdateManagerEventQueue);

			// Stop the update task
			vTaskDelete(g_canTaskHandle);

			// Queue the restart of the display
			QueueEvent_t event;
			event.command = RESTART_DISPLAY;
			event.parameter = (void*)&senderId;
			event.parameterLength = sizeof(uint8_t);
			xQueueSend(g_mainEventQueue, &event, portMAX_DELAY);

			continue;
		}
	}
}

static void eventTask(void* p_param)
{
	if (!g_updateAvailable) {
		vTaskDelete(NULL);
	}

	// Event Queue
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_canUpdateManagerEventQueue, &queueEvent, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Initialize update
		if (queueEvent.command == START_UPDATE_FOR_DISPLAY) {
			prepareUpdate(&queueEvent);
		}

		// Transmit the update file
		if (queueEvent.command == TRANSMIT_UPDATE) {
			transmitUpdate(&queueEvent);
		}

		// Execute the update
		if (queueEvent.command == EXECUTE_UPDATE) {
			executeUpdate(&queueEvent);
		}
	}
}

/*
 *	Private functions
 */
static void prepareUpdate(const QueueEvent_t* p_queueEvent)
{
	// Start the can task
	if (xTaskCreate(canTask, "CanUpdateManagerCanTask", 2048 * 4, p_queueEvent->parameter, 2,
	                &g_canTaskHandle) != pdPASS) {
		ESP_LOGE("CanUpdateManager", "Couldn't create can task!");

		return;
	}

	/*
	 * Tell the display to brace itself for the update
	 */
	// Create the CAN frame
	TwaiFrame_t frame;

	// Set the com id
	frame.buffer[0] = *(uint8_t*)p_queueEvent->parameter;

	// Set the update file size
	frame.buffer[1] = g_fileSizeB >> 24;
	frame.buffer[2] = g_fileSizeB >> 16;
	frame.buffer[3] = g_fileSizeB >> 8;
	frame.buffer[4] = g_fileSizeB;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_PREPARE_UPDATE, 5);

	// Send the frame
	canQueueFrame(&frame);
}

static void transmitUpdate(const QueueEvent_t* p_queueEvent)
{
	if (g_file == NULL) {
		return;
	}

	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Logging
	if (g_bytesTransmitted % UPDATE_BLOCK_SIZE_B * 1000 == 0) {
		ESP_LOGI("CanUpdateManager", "Transmitted %d bytes of total %d bytes", g_bytesTransmitted, g_fileSizeB);
	}

	// Read the bytes
	uint8_t amountReadBytes = 0;
	if (g_bytesTransmitted + UPDATE_BLOCK_SIZE_B <= g_fileSizeB) {
		amountReadBytes = fread(&frame.buffer[1], sizeof(uint8_t), UPDATE_BLOCK_SIZE_B, g_file);
	}
	else {
		// Logging
		ESP_LOGI("CanUpdateManager", "Transmitting last %d bytes", g_fileSizeB - g_bytesTransmitted);

		amountReadBytes = fread(&frame.buffer[1], sizeof(uint8_t), g_fileSizeB - g_bytesTransmitted,
		                        g_file);
	}
	g_bytesTransmitted += amountReadBytes;

	// Set the comid
	frame.buffer[0] = *(uint8_t*)p_queueEvent->parameter;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_TRANSMIT_UPDATE_FILE, amountReadBytes + 1);

	// Send the frame
	canQueueFrame(&frame);
}

static void executeUpdate(const QueueEvent_t* p_queueEvent)
{
	// Logging
	ESP_LOGI("CanUpdateManager", "Transmitting completed. Executing update which may take a while");

	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the com id
	frame.buffer[0] = *(uint8_t*)p_queueEvent->parameter;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_EXECUTE_UPDATE, 1);

	// Send the frame
	canQueueFrame(&frame);
}

static void loadUpdateFileSize()
{
	if (!g_updateAvailable) {
		return;
	}

	// Try to open the file as binary
	g_file = filesystemOpenFile(g_fileName, "rb", SD_CARD);
	if (g_file == NULL) {
		return;
	}

	// Jump to the end
	fseek(g_file, 0, SEEK_END);
	g_fileSizeB = ftell(g_file);

	// Calculate the amount of blocks we need to transmit
	uint32_t updateFileBlocks = g_fileSizeB % UPDATE_BLOCK_SIZE_B > 0 ? 1 : 0;
	updateFileBlocks += g_fileSizeB / UPDATE_BLOCK_SIZE_B;

	// Jump back to the top of the file
	fseek(g_file, 0, SEEK_SET);

	ESP_LOGI("CanUpdateManager", "Update file size: %d, corresponds to %d blocks", g_fileSizeB, updateFileBlocks);
}

/*
 *	Public function implementations
*/
void canUpdateManagerInit()
{
	/*
	 *	Start the update task
	 */
	// Register the update task queue to the CAN bus
	canRegisterRxCbQueue(&g_canUpdateManagerCanQueue);

	// Start the event task
	if (xTaskCreate(eventTask, "CanUpdateManagerEventTask", 2048 * 4, NULL, 2, &g_eventTaskHandle) != pdPASS) {
		ESP_LOGE("CanUpdateManager", "Couldn't create event task!");

		return;
	}
}

bool displayUpdateCanIsUpdateAvailable()
{
	/*
	 *	Check filesystem
	 */
	// Get a list of files in the SD Card updates folder
	uint16_t amountOfFiles = 0;
	struct dirent** files = filesystemSDCardListDirectoryContents(UPDATE_SDCARD_FOLDER, &amountOfFiles);
	if (files == NULL || amountOfFiles == 0) {
		return false;
	}

	/*
	 *	Check all files for the update file name pattern
	 */
	// Iterate through all files
	int major, minor, patch;
	char suffix[256];
	for (uint16_t i = 0; i < amountOfFiles; i++) {
		if (g_updateAvailable) {
			free(*(files + i));
			continue;
		}

		// Current file name
		const struct dirent* file = *(files + i);
		if (file == NULL) {
			continue;
		}

		// Check if file name matches pattern
		const int matches = sscanf(file->d_name, UPDATE_FILE_NAME_PATTERN, &major, &minor, &patch, suffix);
		if (matches != 4) {
			free(*(files + i));
			continue;
		}

		// Check if the suffix is minimum 7 chars long
		if (strlen(suffix) < 7) {
			free(*(files + i));
			continue;
		}

		// Update file found, save it
		snprintf(g_fileName, strlen(UPDATE_SDCARD_FOLDER) + strlen("/") + strlen(file->d_name), "%s/%s",
		         UPDATE_SDCARD_FOLDER, file->d_name);
		g_updateAvailable = true;
	}

	return g_updateAvailable;
}

bool displayUpdateCanStart(const uint8_t comId)
{
	if (comId == 0 || !g_updateAvailable) {
		return false;
	}

	// Get the update file size
	if (g_fileSizeB == 0) {
		loadUpdateFileSize();
	}
	if (g_fileSizeB == 0) {
		ESP_LOGE("CanUpdateManager", "Update file size was 0.");

		return false;
	}

	return true;
}
