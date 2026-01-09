#include "DisplayUpdate.h"

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
#define UPDATE_PART_SIZE_B 7
#define UPDATE_SDCARD_FOLDER "updates"
#define UPDATE_FILE_NAME_PATTERN "Update_Display_%d.%d.%d-%255s" // e.g. "Update_2.0.1-bfe634f"

/*
 *	Private variables
 */
static bool g_updateFileAvailable = false;
static uint32_t g_updateFileSizeB = 0;
static uint32_t g_updateFileAmountOfParts = 0;
static char* g_updateFileName = NULL;
static FILE* g_updateFile = NULL;

static bool g_runUpdateTask = false;
static uint32_t g_currentUpdatePart = 0;
static uint8_t g_sizeOfLastUpdatePartB = 0;
static uint32_t g_totalBytesTransmitted = 0;

/*
 *	Prototypes
 */
static void handleCanMessage(const QueueEvent_t* p_queueEvent, const uint8_t comId);

/*
 *	Private tasks
 */
static void updateTask(void *p_param)
{
	if (!g_updateFileAvailable) {
		vTaskDelete(NULL);
	}

	// Get the com id
	const uint8_t comId = *(uint8_t*)p_param;

	// Event Queue
	QueueEvent_t queueEvent;
	while (g_runUpdateTask) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_updateDisplayEventQueue, &queueEvent, pdMS_TO_TICKS(250))) {
			switch (queueEvent.command) {
				/*
				 *	Received CAN frame
				 */
				case RECEIVED_NEW_CAN_MESSAGE:
				{
					handleCanMessage(&queueEvent, comId);

					break;
				}

				/*
				 *	 Update Commands
				 */
				case INITIALIZE_UPDATE:
				{
					/*
					 * Tell the display to brace itself for the update
					 */
					// Create the CAN answer frame
					TwaiFrame_t frame;

					// Set the com id
					frame.buffer[0] = comId;

					// Set the update file size
					frame.buffer[1] = g_updateFileSizeB >> 24;
					frame.buffer[2] = g_updateFileSizeB >> 16;
					frame.buffer[3] = g_updateFileSizeB >> 8;
					frame.buffer[4] = g_updateFileSizeB;

					// Initiate the frame
					canInitiateFrame(&frame, CAN_MSG_INIT_UPDATE_MODE, g_ownCanComId, 5);

					// Send the frame
					canQueueFrame(&frame);
					break;
				}
				case TRANSMIT_UPDATE:
				{
					if (g_updateFile == NULL) {
						ESP_LOGE("DisplayUpdate", "No Update file open. Aborting update");

						return;
					}

					// Create the CAN answer frame
					TwaiFrame_t frame;

					// Read the bytes
					if (g_totalBytesTransmitted % 7000 == 0)
						ESP_LOGI("DisplayUpdate", "%d, %d", g_totalBytesTransmitted, g_updateFileSizeB);
					if (g_totalBytesTransmitted + UPDATE_PART_SIZE_B <= g_updateFileSizeB) {
						g_sizeOfLastUpdatePartB = fread(&frame.buffer[1], sizeof(uint8_t), UPDATE_PART_SIZE_B, g_updateFile);
					} else {
						ESP_LOGI("> DisplayUpdate", "%d, %d", g_totalBytesTransmitted, g_updateFileSizeB);
						g_sizeOfLastUpdatePartB = fread(&frame.buffer[1], sizeof(uint8_t), g_updateFileSizeB - g_totalBytesTransmitted, g_updateFile);
					}
					g_totalBytesTransmitted += g_sizeOfLastUpdatePartB;

					// Set the comid
					frame.buffer[0] = comId;

					// Initiate the frame
					canInitiateFrame(&frame, CAN_MSG_TRANSMIT_UPDATE_FILE, g_ownCanComId, g_sizeOfLastUpdatePartB + 1);

					// Send the frame
					canQueueFrame(&frame);

					break;
				}
				case EXECUTE_UPDATE:
				{
					// Create the CAN answer frame
					TwaiFrame_t frame;

					// Set the com id
					frame.buffer[0] = comId;

					// Initiate the frame
					canInitiateFrame(&frame, CAN_MSG_EXECUTE_UPDATE, g_ownCanComId, 1);

					// Send the frame
					canQueueFrame(&frame);

					break;
				}
				default: break;
			}
		}
	}

	vTaskDelete(NULL);
}

/*
 *	Private functions
 */
static void checkSdCardForUpdateFile()
{
	if (g_updateFileAvailable) {
		return;
	}

	/*
	 *	Get a list of all available files
	 */
	// Get a list of all files
	uint16_t amountOfFiles = 0;
	struct dirent** files = filesystemSDCardListDirectoryContents(UPDATE_SDCARD_FOLDER, &amountOfFiles);
	if (files == NULL || amountOfFiles == 0) {
		esp_rom_printf("files == NULL || amountOfFiles == 0\n");
		return;
	}

	/*
	 * Check all files for the update file name pattern
	 */
	// Iterate through all files
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
		int major, minor, patch;
		char suffix[256];
		const int matches = sscanf(file->d_name, UPDATE_FILE_NAME_PATTERN, &major, &minor, &patch, suffix);

		// Enough matches?
		if (matches != 4) {
			free(*(files + i));
			continue;
		}

		// Check if the suffix is not empty
		if (strlen(suffix) == 0) {
			free(*(files + i));
			continue;
		}

		// Allocate the needed memory
		const uint16_t length = strlen(UPDATE_SDCARD_FOLDER) + strlen("/") + strlen(file->d_name) + 1;
		g_updateFileName = malloc(length);
		if (g_updateFileName == NULL) {
			free(*(files + i));
			continue;
		}

		// Update file found, save it
		snprintf(g_updateFileName, length, "%s/%s", UPDATE_SDCARD_FOLDER, file->d_name);
		g_updateFileAvailable = true;
	}
}

static void loadUpdateFileSize()
{
	// Reset the file size
	g_updateFileSizeB = 0;

	// Update file available?
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

	// Calculate the amount of parts we need to transmit
	g_updateFileAmountOfParts = g_updateFileSizeB % UPDATE_PART_SIZE_B > 0 ? 1 : 0;
	g_updateFileAmountOfParts += g_updateFileSizeB / UPDATE_PART_SIZE_B;

	// Jump back to the top of the file
	fseek(g_updateFile, 0, SEEK_SET);

	ESP_LOGI("DisplayUpdate", "Need to transmit %d parts. Update size: %d", g_updateFileAmountOfParts, g_updateFileSizeB);
}

static void handleCanMessage(const QueueEvent_t* p_queueEvent, const uint8_t comId)
{
	// Get the frame
	const twai_frame_t recFrame = p_queueEvent->canFrame;

	// Get the message id
	const uint8_t messageId = recFrame.header.id >> CAN_MESSAGE_ID_OFFSET;

	// Get the sender com id
	const uint32_t senderComId = (uint8_t)recFrame.header.id;

	// Is it of the display we update?
	if (senderComId != comId) {
		return;
	}

	// Acknowledge of Initialization of Update
	if (messageId == CAN_MSG_INIT_UPDATE_MODE) {
		// Queue the event
		QueueEvent_t event;
		event.command = TRANSMIT_UPDATE;
		xQueueSend(g_updateDisplayEventQueue, &event, portMAX_DELAY);
		return;
	}

	// Answer to the last transmitted update file part
	if (messageId == CAN_MSG_TRANSMIT_UPDATE_FILE) {
		// Get the part number
		const uint8_t part = *p_queueEvent->canFrame.buffer;

		// Was it the last part of the file?
		if (g_totalBytesTransmitted >= g_updateFileSizeB) {
			// Queue the execution of the update
			QueueEvent_t event;
			event.command = EXECUTE_UPDATE;
			xQueueSend(g_updateDisplayEventQueue, &event, portMAX_DELAY);

			return;
		}

		// Queue the transmission of the next part
		QueueEvent_t event;
		event.command = TRANSMIT_UPDATE;
		xQueueSend(g_updateDisplayEventQueue, &event, portMAX_DELAY);

		return;
	}

	// Acknowledge of the update execution
	if (messageId == CAN_MSG_EXECUTE_UPDATE) {
		ESP_LOGI("DisplayUpdate", "messageId == CAN_MSG_EXECUTE_UPDATE");

		// Stop the update task
		g_runUpdateTask = false;

		// Queue the restart of the display
		QueueEvent_t event;
		event.command = RESTART_DISPLAY;
		event.parameter = (void*)&comId;
		xQueueSend(g_mainEventQueue, &event, portMAX_DELAY);

		return;
	}
}

/*
 *	Public function implementations
 */
bool displayUpdateNegotiate(const uint8_t comId)
{
	if (comId == 0) {
		return false;
	}

	// Check if an update is available
	if (!g_updateFileAvailable) {
		checkSdCardForUpdateFile();
	}

	// Check if a file was found
	if (!g_updateFileAvailable) {
		ESP_LOGW("DisplayUpdate", "No update file found!");

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
	g_runUpdateTask = true;
	TaskHandle_t taskHandle;
	if(xTaskCreate(updateTask, "displayUpdateTask", 2048 * 4, (void*)&comId, 2, &taskHandle) != pdPASS) {
		ESP_LOGE("DisplayUpdate", "Couldn't create update task!");

		return false;
	}

	// And queue the initialization of the update
	QueueEvent_t event;
	event.command = INITIALIZE_UPDATE;
	xQueueSend(g_updateDisplayEventQueue, &event, portMAX_DELAY);

	return true;
}
