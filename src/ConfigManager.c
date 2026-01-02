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
static bool g_displayConfigAvailable = true;
static char g_displayConfigurationBuffer[MAX_CONFIG_SIZE_B];
static cJSON* g_displayConfigurationRoot = NULL;

static bool g_wifiConfigAvailable = true;
static char g_wifiConfigurationBuffer[MAX_CONFIG_SIZE_B];
static cJSON* g_wifiConfigurationRoot = NULL;

/*
 *	Private Functions
 */
static bool fileToJson(const char* p_pfileName, char* p_pbuffer, const uint16_t bufferLen, cJSON** p_proot)
{
	// Open the file
	FILE* file = fileManagerOpenFile(p_pfileName, "r", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_pfileName, CONFIG_PARTITION);
		return false;
	}

	// Zero the buffer
	memset(p_pbuffer, 0, bufferLen);

	// Read everything from the file
	if (fread(p_pbuffer, 1, bufferLen, file) == 0) {
		fclose(file);
		ESP_LOGE("ConfigManager", "Couldn't read from file %s on partition %d", p_pfileName, CONFIG_PARTITION);
		return false;
	}
	fclose(file);

	// Parse it to JSON
	*p_proot = cJSON_Parse(p_pbuffer);
	if (*p_proot == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't parse the content of file %s on partition %d to JSON", p_pfileName, CONFIG_PARTITION);
		return false;
	}
	return true;
}

static bool jsonToFile(const cJSON* p_proot, const char* p_pfileName)
{
	// Open the file
	FILE* file = fileManagerOpenFile(p_pfileName, "w", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("ConfigManager", "Couldn't open file %s on partition %d", p_pfileName, CONFIG_PARTITION);
		return false;
	}

	// Write the JSON configuration to it
	char* jsonFormatted = cJSON_Print(p_proot);
	int result = fprintf(file, "%s", jsonFormatted);
	free(jsonFormatted);

	// Close the file
	fclose(file);

	// Did it fail?
	if (result <= 0) {
		ESP_LOGE("ConfigManager", "Couldn't write the JSON configuration to file %s on partition %d", p_pfileName, CONFIG_PARTITION);
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
	g_displayConfigAvailable = fileToJson(DISPLAY_CONFIG_NAME, g_displayConfigurationBuffer, MAX_CONFIG_SIZE_B,
	                                     &g_displayConfigurationRoot);

	// Load the default config if it failed
	if (!g_displayConfigAvailable) {
		ESP_LOGW("ConfigManager", "Couldn't load display config. Loading default config.");
		g_displayConfigAvailable = fileToJson(DEFAULT_CONFIG_FOLDER "/" DISPLAY_CONFIG_NAME, g_displayConfigurationBuffer,
		                                     MAX_CONFIG_SIZE_B, &g_displayConfigurationRoot);
	}

	/*
	 *	WiFi Config
	 */
	// Try to load the WiFi config
	g_wifiConfigAvailable = fileToJson(WIFI_CONFIG_NAME, g_wifiConfigurationBuffer, MAX_CONFIG_SIZE_B,
	                                  &g_wifiConfigurationRoot);

	// Load the default config if it failed
	if (!g_wifiConfigAvailable) {
		ESP_LOGW("ConfigManager", "Couldn't load wifi config. Loading default config.");
		g_wifiConfigAvailable = fileToJson(DEFAULT_CONFIG_FOLDER "/" WIFI_CONFIG_NAME, g_wifiConfigurationBuffer,
		                                  MAX_CONFIG_SIZE_B, &g_wifiConfigurationRoot);
	}
}

cJSON* getDisplayConfiguration()
{
	if (!g_displayConfigAvailable) {
		return NULL;
	}
	return g_displayConfigurationRoot;
}

bool writeDisplayConfigurationToFile()
{
	return jsonToFile(g_displayConfigurationRoot, DISPLAY_CONFIG_NAME);
}

cJSON* getWifiConfiguration()
{
	if (!g_wifiConfigAvailable) {
		return NULL;
	}

	return g_wifiConfigurationRoot;
}

bool writeWifiConfigurationToFile()
{
	return jsonToFile(g_wifiConfigurationRoot, WIFI_CONFIG_NAME);
}
