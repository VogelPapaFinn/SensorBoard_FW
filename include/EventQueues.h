#pragma once

// espidf includes
#include "esp_twai.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Public typedefs
 */
//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/*
	 * CAN
	 */
	CAN_DRIVER_CRASHED,

	/*
	 *	Main
	 */
	RESTART_DISPLAY,

	/*
	 * Operation
	 */
	READ_SENSOR_DATA,
	SEND_SENSOR_DATA,

	/*
	 *	CanUpdateManager
	 */
	START_UPDATE_FOR_DISPLAY,
	TRANSMIT_UPDATE,
	EXECUTE_UPDATE,
} QueueCommand_t;

//! \brief A typedef struct which is used in the event queues
typedef struct
{
	//! \brief The command of the event
	QueueCommand_t command;

	//! \brief Additional parameters
	void* parameter;

	//! \brief Additional parameters length
	uint16_t parameterLength;
} QueueEvent_t;

/*
 *	CAN queues
 */
//! \brief The Queue used to send CAN frames to the registration manager
extern QueueHandle_t g_registrationManagerCanQueue;

//! \brief The Queue used to send CAN frames to the operation manager
extern QueueHandle_t g_operationManagerCanQueue;

//! \brief T The Queue used to send CAN frames to the can update manager
extern QueueHandle_t g_canUpdateManagerCanQueue;

/*
 *	Event queues
 */
//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t g_mainEventQueue;

//! \brief The Queue used to send events to the operation manager
extern QueueHandle_t g_operationManagerEventQueue;

//! \brief The Queue used to send events to the can update manager
extern QueueHandle_t g_canUpdateManagerEventQueue;

/*
 *	Functions
 */
//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
