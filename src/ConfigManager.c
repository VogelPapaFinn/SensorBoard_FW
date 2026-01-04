#include "ConfigManager.h"

// Project includes
#include "FilesystemDriver.h"

// C includes
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/unistd.h>

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
static char* g_configNames[MAX_AMOUNT_OF_CONFIGS] = { NULL };
static cJSON* g_configJsonRoot[MAX_AMOUNT_OF_CONFIGS] = { NULL };

/*
 *	Private Functions
 */
static bool fileToJson(const char* p_fileName, cJSON** p_root)
{
	// Open the file
	FILE* file = filesystemOpenFile(p_fileName, "r", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}

	// Zero the buffer
	char buffer[MAX_CONFIG_SIZE_B];

	// Read everything from the file
	if (fread(buffer, 1, MAX_CONFIG_SIZE_B, file) == 0) {
		fclose(file);
		ESP_LOGE("ConfigManager", "Couldn't read from file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}
	fclose(file);

	// Parse it to JSON
	*p_root = cJSON_Parse(buffer);
	if (*p_root == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't parse the content of file %s on partition %d to JSON", p_fileName, CONFIG_PARTITION);
		return false;
	}
	return true;
}

static bool jsonToFile(const cJSON* p_root, const char* p_fileName)
{
	// Open the file
	FILE* file = filesystemOpenFile(p_fileName, "w", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_fileName, CONFIG_PARTITION);
		return false;
	}

	// Get the JSON configuration
	char* jsonFormatted = cJSON_Print(p_root);
	if (jsonFormatted == NULL) {
		ESP_LOGE("ConfigManager", "Failed to get cJSON configuration");
		fclose(file);
		return false;
	}

	// Write the JSON configuration to the file
	const int written = fprintf(file, "%s", jsonFormatted);
	free(jsonFormatted);

	// Did it fail?
	if (written <= 0) {
		ESP_LOGE("ConfigManager", "Couldn't write the JSON configuration to file %s on partition %d", p_fileName, CONFIG_PARTITION);
		fclose(file);
		return false;
	}

	// Flush everything
	if (fflush(file) == EOF) {
		ESP_LOGE("ConfigManager", "Failed to flush config to file");
		fclose(file);
		return false;
	}
	if (fsync(fileno(file)) == -1)
	{
		ESP_LOGE("ConfigManager", "Failed to syncfile content to filesystem with errno: %ss", strerror(errno));
		fclose(file);
		return false;
	}

	// Close the file
	if (fclose(file) == EOF) {
		ESP_LOGE("ConfigManager", "Failed to close file (writing failed)");
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
bool configLoadFile(const char* p_name, const ConfigFile_t handleAs)
{
	// Save the name
	const uint8_t length = strlen(p_name) + 1;
	g_configNames[handleAs] = malloc(length);
	if (g_configNames[handleAs] == NULL) {
		return false;
	}
	strlcpy(g_configNames[handleAs], p_name, length);

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
	bool result = fileToJson(p_name, &g_configJsonRoot[handleAs]);

	// If it failed, try to load the default config
	if (!result) {
		ESP_LOGW("ConfigManager", "Couldn't load config of file %s. Loading default config.", p_name);

		// Build the default config path
		const char* path = buildDefaultConfigPath(p_name);

		// Try to load the file
		result = fileToJson(p_name, &g_configJsonRoot[handleAs]);;

		// Free the memory
		free((void*)path);

		// Save if it was successful
		g_configAvailable[handleAs] = result;

		// Then return result
		return result;
	}

	// Save if it was successful
	g_configAvailable[handleAs] = result;

	// Logging
	ESP_LOGI("ConfigManager", "Successfully loaded config file %s", p_name);

	return true;
}

cJSON** configGet(const ConfigFile_t config)
{
	// Check if the config is loaded
	if (!g_configAvailable[config]) {
		return NULL;
	}

	// If so, return the JSON root object
	return &g_configJsonRoot[config];
}

bool configWriteToFile(const ConfigFile_t config)
{
	// Check if the config is loaded
	if (!g_configAvailable[config]) {
		return false;
	}

	return jsonToFile(g_configJsonRoot[config], g_configNames[config]);
}
