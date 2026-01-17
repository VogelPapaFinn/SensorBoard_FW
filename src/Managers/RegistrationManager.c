#include "Managers/RegistrationManager.h"

// Project includes
#include "Display.h"
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
//! \brief Task used to receive and handle CAN frames
//! \param p_param Unused parameters
static void canTask(void* p_param);

//! \brief Cb function of timeout timer. Broadcasts a request via CAN for all displays to register themselves
//! \param p_arg Unused arguments
static void broadcastRegistrationRequestCb(void* p_arg);

/*
 *	Private variables
 */
//! \brief Task handle of the CAN task
static TaskHandle_t g_canTaskHandle;

//! \brief Bool indicating if the operation mode was already entered
static bool g_operationAlreadyEntered = false;

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
static void canTask(void* p_param)
{
	// Wait for new queue events
	TwaiFrame_t rxFrame;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(g_registrationManagerCanQueue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		// Get the frame id
		const uint8_t frameId = rxFrame.espidfFrame.header.id >> CAN_FRAME_ID_OFFSET;

		// Get the sender com id
		const uint8_t senderId = rxFrame.espidfFrame.header.id & 0x1FFFFF; // Zero top 8 bits

		// We received a registration request
		if (frameId == CAN_MSG_REGISTRATION && rxFrame.espidfFrame.header.dlc >= 6) {
			/*
			 *	Register it internally
			 */
			// Pass it to the Display Manager
			const DisplayConfig_t* config = displayRegister(rxFrame.buffer);
			if (config == NULL) {
				continue;
			}


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
			frame.buffer[6] = config->comId; // NOLINT

			// Insert the screen
			frame.buffer[7] = config->screen; // NOLINT

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

			/*
			 *	Request the firmware version
			 */
			// Insert the UUID
			frame.buffer[0] = config->comId; // NOLINT

			// Initiate the frame
			canInitiateFrame(&frame, CAN_MSG_REQUEST_FIRMWARE_VERSION, 1);

			// Send the frame
			canQueueFrame(&frame);

			/*
			 *	Enter operation mode if needed
			 */
			// All displays registered?
			if (displayAllRegistered() && !g_operationAlreadyEntered) {
				g_operationAlreadyEntered = true;
				ESP_LOGI("RegistrationManager", "All displays registered themselves. Entering operation mode");

				// Stop and delete the registration timer
				esp_timer_stop(g_registrationTimerHandle);
				esp_timer_delete(g_registrationTimerHandle);

				// Enter the operation mode
				operationManagerInit();

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
