#pragma once

// C includes
#include <stdbool.h>

// espidf includes
#include "../../esp-idf/components/json/cJSON/cJSON.h"

/*
 *	Defines
 */
#define MAX_CONFIG_SIZE_B 1024
#define DISPLAY_CONFIG_NAME "displays_config.json"
#define WIFI_CONFIG_NAME "wifi_config.json"

/*
 *	Functions
 */
void configManagerInit();

cJSON* getDisplayConfiguration();

bool writeDisplayConfigurationToFile();

cJSON* getWifiConfiguration();

bool writeWifiConfigurationToFile();