#include "Managers/RegistrationManager.h"

// Project includes
#include "DisplayCenter.h"
#include "Managers/OperationManager.h"
#include "can.h"

// espidf includes
#include <esp_log.h>
#include <esp_timer.h>

/*
 *	Private defines
 */
#define REGISTRATION_REQUEST_INTERVAL_MICROS (1000 * 1000) // 1000 milliseconds

/*
 *	Prototypes
 */
static void broadcastRegistrationRequestCb(void* p_arg);

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_canTaskHandle;

//! \brief Handle of the timer which is used in the registration process to periodically
//! send the registration request
static esp_timer_handle_t g_registrationTimerHandle;

//! \brief The config of the timer which is used in the registration process to periodically
//! send the registration request
static const esp_timer_create_args_t g_registrationTimerConfig = {.callback = &broadcastRegistrationRequestCb,
                                                                  .name = "Display Registration Timer"};

/*
 *	Tasks and ISRs
 */
//! \brief Task used to receive and handle CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param)
{
	// Wait for new queue events
	twai_frame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_registrationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Get the frame id
		const uint8_t frameId = rxFrame.header.id >> CAN_FRAME_ID_OFFSET;

		// Get the sender com id
		uint8_t senderId = rxFrame.header.id & 0x1FFFFF; // Zero top 8 bits

		// We received a registration request
		if (frameId == CAN_MSG_REGISTRATION && rxFrame.buffer_len >= 6) {
			/*
			 *	Register it internally
			 */
			// Pass it to the Display Manager
			uint8_t screen;
			displayRegisterWithUUID(rxFrame.buffer, &senderId, &screen);

			/*
			 *	Send the com id and the screen type back
			 */
			// Create the CAN answer frame
			TwaiFrame_t frame;

			// Insert the UUID
			frame.buffer[0] = *(rxFrame.buffer + 0); // NOLINT
			frame.buffer[1] = *(rxFrame.buffer + 1); // NOLINT
			frame.buffer[2] = *(rxFrame.buffer + 2); // NOLINT
			frame.buffer[3] = *(rxFrame.buffer + 3); // NOLINT
			frame.buffer[4] = *(rxFrame.buffer + 4); // NOLINT
			frame.buffer[5] = *(rxFrame.buffer + 5); // NOLINT

			// Insert the com id
			frame.buffer[6] = senderId; // NOLINT

			// Insert the screen
			frame.buffer[7] = screen; // NOLINT

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_COMID_ASSIGNATION, 8);

			// Send the frame
			canQueueFrame(&frame);

			// Logging
			ESP_LOGI("RegistrationManager", "Sending ID '%d' and screen '%d' to UUID '%d-%d-%d-%d-%d-%d'",
			         frame.buffer[6],
			         frame.buffer[7], frame.buffer[0],
			         frame.buffer[1],
			         frame.buffer[2], frame.buffer[3], frame.buffer[4], frame.buffer[5]);

			// All displays registered?
			if (displayAllRegistered()) {
				ESP_LOGI("RegistrationManager", "All displays registered themselves. Entering operation mode");

				// Enter the operation mode
				operationManagerInit();

				// Delete the task
				vTaskDelete(NULL);

				continue;
			}
		}
	}
}

static void broadcastRegistrationRequestCb(void* p_arg)
{
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_REGISTRATION, 0);

	// Send the frame
	canQueueFrame(&frame);
}

/*
 *	Public function implementations
 */
bool registrationManagerInit()
{
	// Register to the CAN rx cb
	if (!canRegisterRxCbQueue(&g_registrationManagerCanQueue)) {
		ESP_LOGE("RegistrationManager", "Couldn't register rx cb queue");

		return false;
	}

	// Start the can task
	if (xTaskCreate(canTask, "RegistrationManagerCanTask", 2048 * 4, NULL, 0, &g_canTaskHandle) != pdPASS) {
		ESP_LOGE("RegistrationManager", "Couldn't create CAN task!");

		return false;
	}

	// Create the registration timer
	esp_timer_create(&g_registrationTimerConfig, &g_registrationTimerHandle);

	// Then start the timer
	esp_timer_start_periodic(g_registrationTimerHandle, (uint64_t)REGISTRATION_REQUEST_INTERVAL_MICROS);

	return true;
}

void registrationManagerDestroy()
{
	// Stop and delete the registration timer
	esp_timer_stop(g_registrationTimerHandle);
	esp_timer_delete(g_registrationTimerHandle);

	// Unregister from the CAN rx cb
	canUnregisterRxCbQueue(&g_registrationManagerCanQueue);
}
