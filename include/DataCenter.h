#pragma once

// Project includes
#include "Global.h"

// C includes
#include <stdint.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*
 *	Device Stuff
 */
typedef struct
{
	//! \brief Is the display connected via CAN?
	bool connected;

	//! \brief The firmware version
	char* firmwareVersion;

	//! \brief The HW UUID
	char* uuid;

	//! \brief The assigned COM ID
	uint8_t comId;

	//! \brief The screen it displays: 0 = temperature, 1 = speed, 2 = RPM
	uint8_t screen;

	//! \brief The current WiFi status
	char* wifiStatus;
} Display_t;

/*
 *	Sensor Stuff
 */

//! \brief The speed of the car
extern uint8_t vehicleSpeed;

//! \brief The RPM of the car
extern uint16_t vehicleRPM;

//! \brief The fuel level (%) of the car
extern uint8_t fuelLevelInPercent;

//! \brief The water temperature of the car
extern uint8_t waterTemp;

//! \brief Do we have oil pressure?
extern bool oilPressure;

//! \brief Is the left indicator turned on
extern bool leftIndicator;

//! \brief Is the right indicator turned on
extern bool rightIndicator;

/*
 *	Connection Stuff
 */

//! \brief Amount of connected displays. Used for assigning the COM IDs
extern uint8_t amountOfConnectedDisplays;

//! \brief The IP address we get assigned when joining an AP
extern uint8_t ipAddress[4];

/*
 *	Public functions
 */
void dataCenterInit();

bool registerDataCenterCbQueue(QueueHandle_t* queueHandle);

void broadcastSensorDataChanged();

void broadcastDisplayStatiChanged();

Display_t* getDisplayStatiObjects();

char* getAllDisplayStatiAsJSON(void);
