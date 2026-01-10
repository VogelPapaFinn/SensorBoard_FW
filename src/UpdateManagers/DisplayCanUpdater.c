#include "../../include/UpdateManagers/DisplayCanUpdater.h"

// Project includes
#include "FilesystemDriver.h"
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
 *	Private variables
 */
//! \brief Bool indicating if an update file was found
static bool g_updateFileAvailable = false;

//! \brief The update size of the update file in bytes
static uint32_t g_updateFileSizeB = 0;

//! \brief The name of the update file
static char g_updateFileName[UPDATE_FILE_MAX_NAME_LENGTH] = {'\0'};

//! \brief Stream to read from the update file
static FILE* g_updateFile = NULL;

//! \brief Task handle of the update task
static TaskHandle_t g_updateTaskHandle;

//! \brief Amount of total bytes transmitted
static uint32_t g_totalBytesTransmitted = 0;

/*
 *	Prototypes
 */
//! \brief FreeRTOS task which handles all events in the event queue
//! \param p_param Unused parameters
static void updateTask(void* p_param);

//! \brief Handles all incoming CAN frames
//! \param p_queueEvent A pointer to the received CAN frame
static void handleCanFrame(const QueueEvent_t* p_queueEvent);

//! \brief Handles the initialization of the update
//! \param p_queueEvent A pointer to the queue event
static void initializeUpdate(const QueueEvent_t* p_queueEvent);

//! \brief Handles the transmitting of the update
//! \param p_queueEvent A pointer to the queue event
static void transmitUpdate(const QueueEvent_t* p_queueEvent);

//! \brief Handles the execution of the update
//! \param p_queueEvent A pointer to the queue event
static void executeUpdate(const QueueEvent_t* p_queueEvent);

/*
 *	Private tasks
 */
static void updateTask(void* p_param)
{
	if (!g_updateFileAvailable) {
		vTaskDelete(NULL);
	}

	// Event Queue
	QueueEvent_t queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_displayCanUpdateEventQueue, &queueEvent, pdMS_TO_TICKS(250))) {
			// New CAN message
			if (queueEvent.command == RECEIVED_NEW_CAN_MESSAGE) {
				handleCanFrame(&queueEvent);
			}

			// Initialize update
			if (queueEvent.command == INITIALIZE_UPDATE) {
				initializeUpdate(&queueEvent);
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
}

/*
 *	Private functions
 */
static void handleCanFrame(const QueueEvent_t* p_queueEvent)
{
	// Get the frame
	const twai_frame_t recFrame = p_queueEvent->canFrame;

	// Get the message id
	const uint8_t messageId = recFrame.header.id >> CAN_MESSAGE_ID_OFFSET;

	// Get the sender com id
	const uint32_t senderComId = (uint8_t)recFrame.header.id;

	// Is it of the display we update?
	if (senderComId != *(uint8_t*)p_queueEvent->parameter) {
		return;
	}

	// Acknowledge of Initialization of Update
	if (messageId == CAN_MSG_INIT_UPDATE_MODE) {
		// Queue the event
		QueueEvent_t event;
		event.command = TRANSMIT_UPDATE;
		event.parameter = p_queueEvent->parameter;
		event.parameterLength = sizeof(uint8_t);
		xQueueSend(g_displayCanUpdateEventQueue, &event, portMAX_DELAY);
		return;
	}

	// Answer to the last transmitted update file part
	if (messageId == CAN_MSG_TRANSMIT_UPDATE_FILE) {
		// Was it the last part of the file?
		if (g_totalBytesTransmitted >= g_updateFileSizeB) {
			// Queue the execution of the update
			QueueEvent_t event;
			event.command = EXECUTE_UPDATE;
			event.parameter = p_queueEvent->parameter;
			event.parameterLength = sizeof(uint8_t);
			xQueueSend(g_displayCanUpdateEventQueue, &event, portMAX_DELAY);

			return;
		}

		// Queue the transmission of the next part
		QueueEvent_t event;
		event.command = TRANSMIT_UPDATE;
		event.parameter = p_queueEvent->parameter;
		event.parameterLength = sizeof(uint8_t);
		xQueueSend(g_displayCanUpdateEventQueue, &event, portMAX_DELAY);

		return;
	}

	// Acknowledge of the update execution
	if (messageId == CAN_MSG_EXECUTE_UPDATE) {
		// Unregister the update queue from the CAN
		canUnregisterRxCbQueue(&g_displayCanUpdateEventQueue);

		// Stop the update task
		vTaskDelete(g_updateTaskHandle);

		// Queue the restart of the display
		QueueEvent_t event;
		event.command = RESTART_DISPLAY;
		event.parameter = p_queueEvent->parameter;
		event.parameterLength = sizeof(uint8_t);
		xQueueSend(g_mainEventQueue, &event, portMAX_DELAY);

		return;
	}
}

static void initializeUpdate(const QueueEvent_t* p_queueEvent)
{
	/*
	 * Tell the display to brace itself for the update
	 */
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the com id
	frame.buffer[0] = *(uint8_t*)p_queueEvent->parameter;

	// Set the update file size
	frame.buffer[1] = g_updateFileSizeB >> 24;
	frame.buffer[2] = g_updateFileSizeB >> 16;
	frame.buffer[3] = g_updateFileSizeB >> 8;
	frame.buffer[4] = g_updateFileSizeB;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_INIT_UPDATE_MODE, 5);

	// Send the frame
	canQueueFrame(&frame);
}

static void transmitUpdate(const QueueEvent_t* p_queueEvent)
{
	if (g_updateFile == NULL) {
		return;
	}

	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Logging
	if (g_totalBytesTransmitted % UPDATE_BLOCK_SIZE_B * 1000 == 0) {
		ESP_LOGI("DisplayUpdate", "Transmitted %d bytes of total %d bytes", g_totalBytesTransmitted, g_updateFileSizeB);
	}

	// Read the bytes
	uint8_t amountReadBytes = 0;
	if (g_totalBytesTransmitted + UPDATE_BLOCK_SIZE_B <= g_updateFileSizeB) {
		amountReadBytes = fread(&frame.buffer[1], sizeof(uint8_t), UPDATE_BLOCK_SIZE_B, g_updateFile);
	}
	else {
		// Logging
		ESP_LOGI("DisplayUpdate", "Transmitting last %d bytes", g_updateFileSizeB - g_totalBytesTransmitted);

		amountReadBytes = fread(&frame.buffer[1], sizeof(uint8_t), g_updateFileSizeB - g_totalBytesTransmitted,
		                                g_updateFile);
	}
	g_totalBytesTransmitted += amountReadBytes;

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
	ESP_LOGI("DisplayUpdate", "Transmitting completed. Executing update which may take a while");

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
	if (!g_updateFileAvailable) {
		return;
	}

	// Try to open the file as binary
	g_updateFile = filesystemOpenFile(g_updateFileName, "rb", SD_CARD);
	if (g_updateFile == NULL) {
		return;
	}

	// Jump to the end
	fseek(g_updateFile, 0, SEEK_END);
	g_updateFileSizeB = ftell(g_updateFile);

	// Calculate the amount of blocks we need to transmit
	uint32_t updateFileBlocks = g_updateFileSizeB % UPDATE_BLOCK_SIZE_B > 0 ? 1 : 0;
	updateFileBlocks += g_updateFileSizeB / UPDATE_BLOCK_SIZE_B;

	// Jump back to the top of the file
	fseek(g_updateFile, 0, SEEK_SET);

	ESP_LOGI("DisplayUpdate", "Update file size: %d, corresponds to %d blocks", g_updateFileSizeB, updateFileBlocks);
}

/*
 *	Public function implementations
*/
bool displayUpdateCanIsUpdateAvailable()
{
	/*
	 *	Get a list of all available files
	 */
	// Get a list of files in the SD Card updates folder
	uint16_t amountOfFiles = 0;
	struct dirent** files = filesystemSDCardListDirectoryContents(UPDATE_SDCARD_FOLDER, &amountOfFiles);
	if (files == NULL || amountOfFiles == 0) {
		return false;
	}

	/*
	 * Check all files for the update file name pattern
	 */
	// Iterate through all files
	int major, minor, patch;
	char suffix[256];
	for (uint16_t i = 0; i < amountOfFiles; i++) {
		if (g_updateFileAvailable) {
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
		snprintf(g_updateFileName, strlen(UPDATE_SDCARD_FOLDER) + strlen("/") + strlen(file->d_name), "%s/%s",
		         UPDATE_SDCARD_FOLDER, file->d_name);
		g_updateFileAvailable = true;
	}

	return g_updateFileAvailable;
}

bool displayUpdateCanStart(const uint8_t comId)
{
	if (comId == 0 || !g_updateFileAvailable) {
		return false;
	}

	// Get the update file size
	if (g_updateFileSizeB == 0) {
		loadUpdateFileSize();
	}
	if (g_updateFileSizeB == 0) {
		ESP_LOGE("DisplayUpdate", "Update file size was 0.");

		return false;
	}

	/*
	 *	Start the update task
	 */
	// Register the update task queue to the CAN bus
	canRegisterRxCbQueue(&g_displayCanUpdateEventQueue);

	// Start the update task
	if (xTaskCreate(updateTask, "displayUpdateTask", 2048 * 4, NULL, 2, &g_updateTaskHandle) != pdPASS) {
		ESP_LOGE("DisplayUpdate", "Couldn't create update task!");

		return false;
	}

	// And queue the initialization of the update
	QueueEvent_t event;
	event.command = INITIALIZE_UPDATE;
	event.parameter = (void*)&comId;
	event.parameterLength = sizeof(comId);
	xQueueSend(g_displayCanUpdateEventQueue, &event, portMAX_DELAY);

	return true;
}
