// Project includes
#include "ComCenter.h"
#include "Global.h"
#include "can.h"

// espidf includes
#include "esp_timer.h"

#include "esp_twai.h"
#include "esp_twai_onchip.h"

/*
 *	Prototypes
 */
void IRAM_ATTR canMessageDistributorTask(void* arg);
void IRAM_ATTR requestHwUUIDsISR(void* arg);

/*
 *	Private variables
 */
//! \brief The task handle of the CAN rx cb
TaskHandle_t canRxTaskHandle_;

//! \brief Pointer to the CAN node
twai_node_handle_t* canNodeHandle_ = NULL;

esp_timer_handle_t uuidTimerHandle;
const esp_timer_create_args_t uuidTimerConf = {.callback = &requestHwUUIDsISR, .name = "Request HW UUIDs Timer"};

/*
 *	Tasks and ISRs
 */
//! \brief Used to distribute received CAN messages through the code base
void canMessageDistributorTask(void* arg)
{
	while (true) {
		// Blocks until we are notified
		xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);

		// Get the new message
		const twai_frame_t message = getLastReceivedMessage();

		// Decide on its type
		switch (message.header.id) {
			case CAN_MSG_REGISTER_HW_UUID:
				// Logging
				loggerInfo("Received HW UUID: %d", message.buffer[0]);

				// Do we already know this UUID?
				for (int i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
					// Was a UUID already received?
					if (knownHwUUIDs[i] == 0) {
						// Then save it
						knownHwUUIDs[i] = message.buffer[0];
						break;
					}
					if (knownHwUUIDs[i] == message.buffer[0]) {
						break;
					}
				}
				break;

			default:
				break;
		}
	}
}

//! \brief Connected to a timeout to send UUID requests until we received all of them
void requestHwUUIDsISR(void* arg)
{
	// Do we received all HW UUIDs?
	bool allUUIDsReceived = true;
	for (int i = 0; i < sizeof(knownHwUUIDs); i++) {
		if (knownHwUUIDs[i] == 0) {
			// No
			allUUIDsReceived = false;
			break;
		}
	}

	// Do we need to rerequest?
	if (!allUUIDsReceived) {
		// Add the Register HW UUID Request event to the queue
		QUEUE_EVENT_T registerHwUUID;
		registerHwUUID.command = QUEUE_CMD_MAIN_REQUEST_UUID;
		xQueueSend(mainEventQueue, &registerHwUUID, 0);
	}
	else {
		// Otherwise delete the timer
		esp_timer_delete(uuidTimerHandle);
		uuidTimerHandle = NULL;
	}
}

/*
 *	Private functions
 */
bool startCommunicationCenter()
{
	bool success = true;

	// Create the CAN RX Task
	const BaseType_t taskSuccess = xTaskCreate(canMessageDistributorTask, "CAN_RX_Distributor", 4096 / 4, NULL,
											   tskIDLE_PRIORITY, &canRxTaskHandle_);
	success &= taskSuccess;

	// Initialize the can node
	canNodeHandle_ = initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	success &= canNodeHandle_ != NULL;

	// Register the rx callback
	success &= registerMessageReceivedCb(&canRxTaskHandle_);

	// Enable the node
	success &= enableCanNode();

	return success == pdPASS;
}
