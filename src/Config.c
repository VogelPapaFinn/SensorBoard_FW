#include "Config.h"

// Project includes
#include "../include/Drivers/FilesystemDriver.h"

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

/*
 *	Private Functions
 */
static bool loadJsonFromFile(ConfigFile_t* p_config)
{
	// Open the file
	FILE* file = filesystemOpenFile(p_config->path, "r", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("Config", "Couldn't open file %s on partition %d", p_config->path, CONFIG_PARTITION);
		return false;
	}

	// Zero the buffer
	char buffer[MAX_CONFIG_SIZE_B];

	// Read everything from the file
	if (fread(buffer, 1, MAX_CONFIG_SIZE_B, file) == 0) {
		fclose(file);
		ESP_LOGE("Config", "Couldn't read from file %s on partition %d", p_config->path, CONFIG_PARTITION);
		return false;
	}
	fclose(file);

	// Parse it to JSON
	p_config->jsonRoot = cJSON_Parse(buffer);
	if (p_config->jsonRoot == NULL) {
		ESP_LOGE("Config", "Couldn't parse the content of file %s on partition %d to JSON", p_config->path, CONFIG_PARTITION);
		return false;
	}
	return true;
}

static bool saveJsonToFile(ConfigFile_t* p_config)
{
	// Open the file
	FILE* file = filesystemOpenFile(p_config->path, "w", CONFIG_PARTITION);

	// Did that work?
	if (file == NULL) {
		ESP_LOGE("Config", "Couldn't open file %s on partition %d", p_config->path, CONFIG_PARTITION);
		return false;
	}

	// Get the JSON configuration
	char* jsonFormatted = cJSON_Print(p_config->jsonRoot);
	if (jsonFormatted == NULL) {
		ESP_LOGE("Config", "Failed to get cJSON configuration");
		fclose(file);
		return false;
	}

	// Write the JSON configuration to the file
	const int written = fprintf(file, "%s", jsonFormatted);
	free(jsonFormatted);

	// Did it fail?
	if (written <= 0) {
		ESP_LOGE("Config", "Couldn't write the JSON configuration to file %s on partition %d", p_config->path, CONFIG_PARTITION);
		fclose(file);
		return false;
	}

	// Flush everything
	if (fflush(file) == EOF) {
		ESP_LOGE("Config", "Failed to flush config to file");
		fclose(file);
		return false;
	}
	if (fsync(fileno(file)) == -1)
	{
		ESP_LOGE("Config", "Failed to syncfile content to filesystem with errno: %ss", strerror(errno));
		fclose(file);
		return false;
	}

	// Close the file
	if (fclose(file) == EOF) {
		ESP_LOGE("Config", "Failed to close file (writing failed)");
		return false;
	}

	return true;
}

/*
 *	Public Function Implementations
 */
bool configLoad(ConfigFile_t* p_config)
{
	if (p_config == NULL) {
		return false;
	}

	// Try to load the config
	if (!loadJsonFromFile(p_config)) {
		ESP_LOGW("Config", "Couldn't load config of file %s. Loading default config.", p_config->path);

		// Build the default config path
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
		char tmpBuffer[MAX_CONFIG_FILE_PATH_LENGTH];
		snprintf(tmpBuffer, MAX_CONFIG_FILE_PATH_LENGTH, "%s/%s", DEFAULT_CONFIG_FOLDER, p_config->path);
		strlcpy(p_config->path, tmpBuffer, MAX_CONFIG_FILE_PATH_LENGTH);
#pragma GCC diagnostic pop

		// Try to load the file
		const bool result = loadJsonFromFile(p_config);

		return result;
	}

	// Logging
	ESP_LOGI("Config", "Successfully loaded config file %s", p_config->path);

	return true;
}

bool configSave(ConfigFile_t* p_config)
{
	if (p_config == NULL || p_config->jsonRoot == NULL) {
		return false;
	}

	return saveJsonToFile(p_config);
}
