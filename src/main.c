// Project includes
#include "ComCenter.h"
#include "Global.h"

// espidf includes
#include <esp_mac.h>
#include <esp_timer.h>

#include "can.h"
#include "can_messages.h"

void app_main(void)
{
	// Create the event queues
	createEventQueues();

	// Start the Communication Center
	startCommunicationCenter();

	// Add the Register HW UUID Request event to the queue
	QUEUE_EVENT_T registerHwUUID;
	registerHwUUID.command = QUEUE_CMD_MAIN_REQUEST_UUID;
	xQueueSend(mainEventQueue, &registerHwUUID, 0);

	// Wait for new queue events
	QUEUE_EVENT_T queueEvent;
	while (1) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(mainEventQueue, &queueEvent, portMAX_DELAY)) {
			switch (queueEvent.command) {
				// Request the HW UUID of all devices
				case QUEUE_CMD_MAIN_REQUEST_UUID:
					// Create the can frame
					twai_frame_t* frame = malloc(sizeof(twai_frame_t));
					memset(frame, 0, sizeof(*frame));
					frame->header.id = CAN_MSG_REQUEST_HW_UUID;
					frame->header.dlc = 1;
					frame->header.ide = false;
					frame->header.rtr = true;
					frame->header.fdf = false;

					// Send the frame
					queueCanBusMessage(frame, true, false);

					// Start the timeout timer
					if (uuidTimerHandle == NULL) {
						esp_timer_create(&uuidTimerConf, &uuidTimerHandle);
					}
					if (!esp_timer_is_active(uuidTimerHandle)) {
						esp_timer_start_once(uuidTimerHandle, 200 * 1000);
					}

					// Debug Logging
					loggerDebug("Send HW UUID Request");
					break;
				default:
					break;
			}
		}

		// Wait 100ms
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}
