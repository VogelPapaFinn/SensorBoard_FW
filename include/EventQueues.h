#pragma once

// espidf includes
#include "esp_twai.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Public Stuff
 */

//! \brief The Queue used to send events to the display manager
extern QueueHandle_t g_displayManagerEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t g_mainEventQueue;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/*
	 * CAN
	 */
	CAN_DRIVER_CRASHED,
	RECEIVED_NEW_CAN_MESSAGE,

	/*
	 * MAIN
	 */
	REQUEST_UUID,
	INIT_OPERATION_MODE,
	READ_SENSOR_DATA,
	RESTART_DISPLAY,
} QueueCommand_t;

//! \brief A typedef struct which is used in the event queues
typedef struct
{
	//! \brief The command of the event
	QueueCommand_t command;

	//! \brief An optional CAN frame
	twai_frame_t canFrame;

	//! \brief Additional parameters
	void* parameter;

	//! \brief Additional parameters length
	uint16_t parameterLength;
} QueueEvent_t;

/*
 *	Functions
 */
//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
