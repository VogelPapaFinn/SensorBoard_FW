#pragma once

// C includes
#include <stdbool.h>

// espidf includes
#include "../../esp-idf/components/json/cJSON/cJSON.h"

/*
 *	Defines
 */
#define MAX_AMOUNT_OF_CONFIGS 8
#define MAX_CONFIG_SIZE_B 1024

/*
 *	Public Typedefs
 */
typedef enum
{
	DISPLAY_CONFIG,
	WIFI_CONFIG
} ConfigFile_t;

/*
 *	Functions
 */
//! \brief Tries to load a config file and saves it under the specified handle
//! \param p_name The name of the file to load
//! \param handleAs The handle under which it should be saved and later on accessed
//! \retval Bool indicating if the config was loaded successfully
bool loadConfigFile(char* p_name, ConfigFile_t handleAs);

//! \brief Returns the cJSON root object of the config
//! \param config The handle of the config
//! \retval A pointer to the cJSON root object or NULL if the config isn't loaded
cJSON* getConfig(ConfigFile_t config);

//! \brief Writes the configuration, which is saved in the cJSON root object, into the file
//! \param config The handle of the config
//! \retval Bool indicating if the write process succeeded
bool writeConfigToFile(ConfigFile_t config);