#pragma once

// C includes
#include <stdbool.h>
#include <stdint.h>

/*
 *	Public typedefs
 */
//! \brief Enum indicating if the WiFi AP should be hosted or joined
typedef enum
{
	HOST_AP,
	JOIN_AP
} WifiType_t;

/*
 *	Global Variables
 */
//! \brief The IP address we get assigned when joining an AP
extern uint8_t g_ipAddress[4];

/*
 *	Public functions
 */
//! \brief Sets the type (e.g. hosting or joining an AP) of the WiFi
//! \param wifiType The type
//! \retval Bool indicating if setting the type worked
bool wifiSetType(WifiType_t wifiType);

//! \brief Returns the current WiFi type
//! \retval The current WiFi type
WifiType_t wifiGetType();

//! \brief Connects to the WiFi (by hosting or joining an AP)
//! \retval Bool indicating if the operation worked
bool wifiConnect();

//! \brief Disconnects from the WiFi
void wifiDisconnect();

//! \brief Returns bool indicating if the WiFi is currently active (doing something, like trying to connect)
//! \retval Bool indicating the active status
bool wifiIsActive();

//! \brief Returns bool indicating if it is currently hosting/connected to an AP
//! \retval Bool indicating the connection status
bool wifiIsConnected();