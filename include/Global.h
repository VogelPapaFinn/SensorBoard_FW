#pragma once

// espidf includes
#include "esp_twai.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

/*
 *	Defines
 */
#define AMOUNT_OF_DISPLAYS 1

/*
 *	Public Stuff
 */

//! \brief The Queue used to send events to the Update Handler
extern QueueHandle_t updateEventQueue;

//! \brief The Queue used to send events to the main application (main.c)
extern QueueHandle_t mainEventQueue;

typedef enum
{
	STATE_INIT,
	STATE_OPERATION,
} STATE_T;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/* CAN */
	QUEUE_RECEIVED_NEW_CAN_MESSAGE,

	/* MAIN */
	QUEUE_REQUEST_UUID,
	QUEUE_INIT_OPERATION_MODE,
	QUEUE_READ_SENSOR_DATA,
	QUEUE_RESTART_DISPLAY,

	/* DATA CENTER */
	QUEUE_SENSOR_DATA_CHANGED,
	QUEUE_DISPLAY_STATI_CHANGED,
} QUEUE_COMMAND_T;

//! \brief A typedef struct which is used in the event Queues
typedef struct
{
	//! \brief The command of the event
	QUEUE_COMMAND_T command;

	//! \brief An optional CAN frame
	twai_frame_t canFrame;

	//! \brief Additional params
	void* parameter;
	uint16_t parameterLength;
} QUEUE_EVENT_T;

/*
 *	Functions
 */
//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
