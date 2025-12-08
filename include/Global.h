#pragma once

// espidf includes
#include "esp_twai.h"

// FreeRTOS include
#include "freertos/FreeRTOS.h"

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

//! \brief The Queue used to send events to the State Machine
extern QueueHandle_t stateMachineEventQueue;

typedef enum
{
	STATE_INIT,
	STATE_OPERATION,
} State_t;

//! \brief A typedef enum that contains commands for all Queues
typedef enum
{
	/* CAN */
	QUEUE_RECEIVED_NEW_CAN_MESSAGE,

	/* MAIN */
	QUEUE_REQUEST_UUID,
	QUEUE_INIT_OPERATION_MODE,
	QUEUE_READ_SENSOR_DATA,
	QUEUE_SEND_SENSOR_DATA,


	QUEUE_CMD_REQUEST_RESET,
	QUEUE_CMD_SEND_SENSOR_DATA,
	QUEUE_CMD_REQUEST_FW_VERSION,

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

extern uint8_t vehicleSpeed;
extern uint16_t vehicleRPM;
extern uint8_t fuelLevelInPercent;
extern uint8_t waterTemp;
extern bool oilPressure;
extern bool leftIndicator;
extern bool rightIndicator;

extern uint8_t ipAddress[4];

/*
 *	Functions
 */
//! \brief Creates the event queues
//! \retval Boolean indicating if the creation was successful
bool createEventQueues();
