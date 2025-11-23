#pragma once

// Project includes
#include "Logger.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

// espidf include
#include "esp_twai.h"

/*
 *	Defines
 */
#define AMOUNT_OF_DISPLAYS 3

/*
 *	Public Stuff
 */

//! \brief The Queue used to send events to the Update Handler
extern QueueHandle_t updateEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t mainEventQueue;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/* MAIN */
	QUEUE_CMD_MAIN_REQUEST_UUID,
	QUEUE_CMD_MAIN_REQUEST_RESET,
	QUEUE_CMD_MAIN_SEND_SENSOR_DATA,
	QUEUE_CMD_MAIN_REQUEST_FW_VERSION,

	/* Update Handler */
	QUEUE_CMD_UPDATE_REQUEST_UPDATE_MODE,
} QUEUE_COMMAND_T;

//! \brief A typedef struct which is used in the event Queues
typedef struct
{
	//! \brief The command of the event
	QUEUE_COMMAND_T command;

	//! \brief An optional CAN frame
	twai_frame_t canFrame;
} QUEUE_EVENT_T;

//! \brief Contains all known HW UUIDs
extern uint8_t knownHwUUIDs[AMOUNT_OF_DISPLAYS];

/*
 *	Functions
 */

//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
