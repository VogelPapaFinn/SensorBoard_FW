#include "ConfigManager.h"

// Project includes
#include "FileManager.h"

// C includes
#include <stdio.h>
#include <string.h>

// espidf includes
#include "esp_log.h"

/*
 *	Private defines
 */
#define DEFAULT_CONFIG_FOLDER "default"

/*
 *	Private Variables
 */
//! \brief Contains a boolean for all configs which can be accessed via the ConfigFile_t value.
//!	If true, the config is loaded and available otherwise it's not.
static bool g_configAvailable[MAX_AMOUNT_OF_CONFIGS] = { false };
static char* g_configBuffer[MAX_AMOUNT_OF_CONFIGS] = { NULL };
static cJSON* g_configJsonRoot[MAX_AMOUNT_OF_CONFIGS] = { NULL };

static bool g_displayConfigAvailable = true;
static char g_displayConfigurationBuffer[MAX_CONFIG_SIZE_B];
static cJSON* g_displayConfigurationRoot = NULL;

static bool g_wifiConfigAvailable = true;
static char g_wifiConfigurationBuffer[MAX_CONFIG_SIZE_B];
static cJSON* g_wifiConfigurationRoot = NULL;

/*
 *	Private Functions
 */
static bool fileToJson(const char* p_fileName, char* p_buffer, const uint16_t bufferLen, cJSON** p_root)
{
	// Open the file
	FILE* file = fileManagerOpenFile(p_fileName, "r", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}

	// Zero the buffer
	memset(p_buffer, 0, bufferLen);

	// Read everything from the file
	if (fread(p_buffer, 1, bufferLen, file) == 0) {
		fclose(file);
		ESP_LOGE("ConfigManager", "Couldn't read from file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}
	fclose(file);

	// Parse it to JSON
	*p_root = cJSON_Parse(p_buffer);
	if (*p_root == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't parse the content of file %s on partition %d to JSON", p_fileName, CONFIG_PARTITION);
		return false;
	}
	return true;
}

static bool jsonToFile(const cJSON* p_root, const char* p_fileName)
{
	// Open the file
	FILE* file = fileManagerOpenFile(p_fileName, "w", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}

	// Write the JSON configuration to it
	char* jsonFormatted = cJSON_Print(p_root);
	int result = fprintf(file, "%s", jsonFormatted);
	free(jsonFormatted);

	// Close the file
	fclose(file);

	// Did it fail?
	if (result <= 0) {
		ESP_LOGE("ConfigManager", "Couldn't write the JSON configuration to file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}
	return true;
}

static char* buildDefaultConfigPath(const char* p_fileName)
{
	// Allocate memory for the final string
	const size_t len = strlen(DEFAULT_CONFIG_FOLDER) + strlen("/") + strlen(p_fileName);
	char* result = malloc(len + 1);
	if (result == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't allocate memory for the default config file path");
		return NULL;
	}

	// Build the string
	snprintf(result, len, "%s/%s", DEFAULT_CONFIG_FOLDER, p_fileName);

	// Return the string
	return result;
}

/*
 *	Public Function Implementations
 */
bool loadConfigFile(char* p_name, const ConfigFile_t handleAs)
{
	// Is there already a buffer?
	if (g_configBuffer[handleAs] != NULL) {
		free(g_configBuffer[handleAs]);
	}

	// Allocate memory for the buffer for the file
	g_configBuffer[handleAs] = malloc(MAX_CONFIG_SIZE_B);
	if (g_configBuffer[handleAs] == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't allocate memory for the configuration buffer of file %s.", p_name);
		return false;
	}

	// Is there already a JSON root?
	if (g_configJsonRoot[handleAs] != NULL) {
		free(g_configJsonRoot[handleAs]);
	}

	// Allocate memory for the JSON root for the file
	g_configJsonRoot[handleAs] = malloc(sizeof(cJSON));
	if (g_configJsonRoot[handleAs] == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't allocate memory for the JSON root object of file %s.", p_name);
		return false;
	}

	// Try to load the config
	bool result = fileToJson(p_name, g_configBuffer[handleAs], MAX_CONFIG_SIZE_B, &g_configJsonRoot[handleAs]);

	// If it failed, try to load the default config
	if (!result) {
		ESP_LOGW("ConfigManager", "Couldn't load config of file %s. Loading default config.", p_name);

		// Build the default config path
		const char* path = buildDefaultConfigPath(p_name);

		// Try to load the file
		result = fileToJson(p_name, g_configBuffer[handleAs], MAX_CONFIG_SIZE_B, &g_configJsonRoot[handleAs]);;

		// Free the memory
		free((void*)path);

		// Then return result
		return result;
	}

	return true;
}

cJSON* getConfig(const ConfigFile_t config)
{
	// Check if the config is loaded
	if (!g_configAvailable[config]) {
		return NULL;
	}

	// If so, return the JSON root object
	return g_configJsonRoot[config];
}

bool writeConfigToFile(const ConfigFile_t config)
{
	// Check if the config is loaded
	if (!g_configAvailable[config]) {
		return false;
	}

	return jsonToFile(g_configJsonRoot[config], g_configBuffer[config]);
}
