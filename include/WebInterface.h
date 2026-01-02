#pragma once

// Project includes
#include "WebAPI.h"

// espidf includes
#include <esp_http_server.h>
#include <esp_log.h>

/*
 *	Public typedefs
 */
typedef enum
{
	HOST_AP,
	JOIN_AP,
	GET_FROM_CONFIG
} WIFI_TYPE;

/*
 *	Public Functions
 */
bool startWebInterface(WIFI_TYPE wifiType);
void webinterfaceSendData(void);
