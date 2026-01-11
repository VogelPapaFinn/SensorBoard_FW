#pragma once

// C includes
#include <stdbool.h>

// espidf includes
#include "../../esp-idf/components/json/cJSON/cJSON.h"

/*
 *	Defines
 */
#define MAX_CONFIG_SIZE_B 1024
#define MAX_CONFIG_FILE_PATH_LENGTH 256

/*
 *	Public Typedefs
 */
//! \brief A struct representing a config file
typedef struct
{
	//! \brief The path of the config file on the filesystem
	char path[MAX_CONFIG_FILE_PATH_LENGTH];

	//! \brief The pointer to the cJSON root object
	cJSON* jsonRoot;
} ConfigFile_t;

/*
 *	Functions
 */
//! \brief Tries to load a config file and saves the cJSON root object in the specified struct
//! \param p_config A pointer to the config file struct
//! \retval Bool indicating if the config was loaded successfully
bool configLoad(ConfigFile_t* p_config);

//! \brief Saves the configuration to the file on the filesystem
//! \param p_config A pointer to the config file struct
//! \retval Bool indicating if the write process was successful
bool configSave(ConfigFile_t* p_config);