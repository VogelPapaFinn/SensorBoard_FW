#include "ConfigManager.h"

// Project includes
#include "FileManager.h"

// C includes
#include <stdio.h>
#include <string.h>

#include "logger.h"

/*
 *	Private defines
 */
#define DEFAULT_CONFIG_FOLDER "default"

/*
 *	Private Variables
 */
static bool displayConfigAvailable_ = true;
static char displayConfigurationBuffer_[MAX_CONFIG_SIZE_B];
static cJSON* displayConfigurationRoot = NULL;

static bool wifiConfigAvailable_ = true;
static char wifiConfigurationBuffer_[MAX_CONFIG_SIZE_B];
static cJSON* wifiConfigurationRoot = NULL;

/*
 *	Private Functions
 */
static bool fileToJson(const char* fileName, char* buffer, const uint16_t bufferLen, cJSON** root)
{
	// Open the file
	FILE* file = fileManagerOpenFile(fileName, "r", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		return false;
	}

	// Zero the buffer
	memset(buffer, 0, bufferLen);

	// Read everything from the file
	if (fread(buffer, 1, bufferLen, file) == 0) {
		fclose(file);
		return false;
	}
	fclose(file);

	// Parse it to JSON
	*root = cJSON_Parse(buffer);
	if (*root == NULL) {
		return false;
	}
	return true;
}

static bool jsonToFile(const cJSON* root, const char* fileName)
{
	// Open the file
	FILE* file = fileManagerOpenFile(fileName, "w", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		return false;
	}

	// Write the JSON configuration to it
	char* jsonFormatted = cJSON_Print(root);
	int result = fprintf(file, "%s", jsonFormatted);
	free(jsonFormatted);

	// Close the file
	fclose(file);

	// Did it fail?
	if (result <= 0) {
		return false;
	}
	return true;
}

/*
 *	Public Function Implementations
 */
void configManagerInit()
{
	/*
	 *	Display Config
	 */
	// Try to load the display config
	displayConfigAvailable_ = fileToJson(DISPLAY_CONFIG_NAME, displayConfigurationBuffer_, MAX_CONFIG_SIZE_B,
	                                     &displayConfigurationRoot);

	// Load the default config if it failed
	if (!displayConfigAvailable_) {
		displayConfigAvailable_ = fileToJson(DEFAULT_CONFIG_FOLDER "/" DISPLAY_CONFIG_NAME, displayConfigurationBuffer_,
		                                     MAX_CONFIG_SIZE_B, &displayConfigurationRoot);
	}

	/*
	 *	WiFi Config
	 */
	// Try to load the WiFi config
	wifiConfigAvailable_ = fileToJson(WIFI_CONFIG_NAME, wifiConfigurationBuffer_, MAX_CONFIG_SIZE_B,
	                                  &wifiConfigurationRoot);

	// Load the default config if it failed
	if (!wifiConfigAvailable_) {
		wifiConfigAvailable_ = fileToJson(DEFAULT_CONFIG_FOLDER "/" WIFI_CONFIG_NAME, wifiConfigurationBuffer_,
		                                  MAX_CONFIG_SIZE_B, &wifiConfigurationRoot);
	}
}

cJSON* getDisplayConfiguration()
{
	if (!displayConfigAvailable_) {
		return NULL;
	}
	return displayConfigurationRoot;
}

bool writeDisplayConfigurationToFile()
{
	return jsonToFile(displayConfigurationRoot, DISPLAY_CONFIG_NAME);
}

cJSON* getWifiConfiguration()
{
	if (!wifiConfigAvailable_) {
		return NULL;
	}

	return wifiConfigurationRoot;
}

bool writeWifiConfigurationToFile()
{
	return jsonToFile(wifiConfigurationRoot, WIFI_CONFIG_NAME);
}
