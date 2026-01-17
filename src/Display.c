#include "Display.h"

// Project includes
#include "../include/Drivers/FilesystemDriver.h"
#include "Config.h"
#include "can.h"

// espidf includes
#include <esp_log.h>

/*
 *	Private Defines
 */
#define AMOUNT_OF_DISPLAYS 1
#define DISPLAY_CONFIG_NAME "displays_config.json"

#define FORMATTED_UUID_LENGTH_B 24

/*
 *	Prototypes
 */
//! \brief Returns the comId assigned to the specified uuid
//! \param p_uuid The uuid array, usually 6 Bytes long
//! \retval The comId or 0 if none was found
static uint8_t getComId(const uint8_t* p_uuid);

//! \brief Tracks the display configuration and loads everything needed
//! \param p_config The configuration that should be tracked
static DisplayConfig_t* trackDisplay(const DisplayConfig_t* p_config);

//! \brief Formats a specified uuid for displaying and handling within the config file
//! \param p_uuid The uuid array, usually 6 Bytes long
//! \param p_buffer The output buffer, where the formatted uuid should be placed into
//! \retval A bool indicating if the write process was successful
static bool getFormattedUuid(const char* p_uuid, char* p_buffer);

//! \brief Debug function to print the content of the configuration file
static void displayPrintConfigFile();

/*
 *	Private Variables
 */
//! \brief The instance of the config file on the filesystem
static ConfigFile_t g_displayConfigFile;

//! \brief Amount of connected displays. Used for assigning the COM IDs
static uint8_t g_amountConnectedDisplays;

//! \brief The array which holds the DisplayConfig_t instances
static DisplayConfig_t g_displayRuntimeConfigs[AMOUNT_OF_DISPLAYS];

/*
 *	Private functions
 */
static uint8_t getComId(const uint8_t* p_uuid)
{
	// Iterate through the list of all com id entries
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is the entry not empty?
		if (g_displayRuntimeConfigs[i].comId == 0) {
			continue;
		}
		// Check if the uuids match
		char formattedUuid1[FORMATTED_UUID_LENGTH_B + 1];
		char formattedUuid2[FORMATTED_UUID_LENGTH_B + 1];
		getFormattedUuid((char*)p_uuid, formattedUuid1);
		getFormattedUuid((char*)g_displayRuntimeConfigs[i].uuid, formattedUuid2);
		if (strcmp(formattedUuid1, formattedUuid2) == 0) {
			return g_displayRuntimeConfigs[i].comId;
		}
	}

	// Nothing found
	return 0;
}

static DisplayConfig_t* trackDisplay(const DisplayConfig_t* p_config)
{
	// Debug printing
	// displayPrintConfigFile();

	/*
	 *	1. Already enough displays registered
	 */
	if (g_amountConnectedDisplays >= AMOUNT_OF_DISPLAYS) {
		ESP_LOGW("Display", "A display tried to register itself, but we already know enough displays");

		return NULL;
	}

	/*
	 *	Occupy runtime configuration
	 */
	DisplayConfig_t* runtimeConfig = NULL;
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		if (g_displayRuntimeConfigs[i].comId == 0) {
			runtimeConfig = &g_displayRuntimeConfigs[i];
			break;
		}
	}
	if (runtimeConfig == NULL) {
		ESP_LOGE("Display", "Failed to occupy a runtime configuration");
		return NULL;
	}

	// Set the com id
	runtimeConfig->comId = ++g_amountConnectedDisplays;

	// Set the uuid
	memcpy(runtimeConfig->uuid, p_config->uuid, UUID_LENGTH_B);

	/*
	 *	2. Unknown display but has config file entry
	 */
	// Get the display configurations array
	cJSON* displayConfigurationsArray = cJSON_GetObjectItem(g_displayConfigFile.jsonRoot, "displayConfigurations");
	if (displayConfigurationsArray == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty display configurations from %d", DISPLAY_CONFIG_NAME);
		return NULL;
	}

	// Get the formatted uuid
	char formattedUuid[FORMATTED_UUID_LENGTH_B + 1];
	getFormattedUuid((char*)&p_config->uuid[0], formattedUuid);

	// Check every entry for a match
	for (uint8_t i = 0; i < (uint8_t)cJSON_GetArraySize(displayConfigurationsArray); i++) {
		const cJSON* jsonEntry = cJSON_GetArrayItem(displayConfigurationsArray, i);
		if (jsonEntry == NULL) {
			continue;
		}

		// Get the entry uuid
		const cJSON* jsonUuid = cJSON_GetObjectItem(jsonEntry, "hwUuid");
		if (jsonUuid == NULL || !cJSON_IsString(jsonUuid) || jsonUuid->valuestring == NULL) {
			continue;
		}

		// Do they match?
		if (strcmp(formattedUuid, jsonUuid->valuestring) == 0) {
			// Get the screen
			const cJSON* jsonScreen = cJSON_GetObjectItem(jsonEntry, "screen");
			if (jsonScreen == NULL || !cJSON_IsNumber(jsonScreen)) {
				continue;
			}

			// Set the screen
			runtimeConfig->screen = jsonScreen->valueint;

			return runtimeConfig;
		}
	}

	/*
	 *	3. Unknown display and no config file entry
	 */
	// Create a new entry
	cJSON* newConfigurationEntry = cJSON_CreateObject();
	cJSON_AddStringToObject(newConfigurationEntry, "hwUuid", formattedUuid);
	cJSON_AddNumberToObject(newConfigurationEntry, "screen", SCREEN_TEMPERATURE);

	// Add the entry to the array
	cJSON_AddItemToArray(displayConfigurationsArray, newConfigurationEntry);

	// Then save the new config
	if (!configSave(&g_displayConfigFile)) {
		ESP_LOGE("DisplayManager", "Couldn't write new display configuration to file");
	}
	else {
		ESP_LOGI("DisplayManager", "Written new display configuration to file");
	}

	// Then set the screen
	runtimeConfig->screen = SCREEN_TEMPERATURE;

	return runtimeConfig;
}

static bool getFormattedUuid(const char* p_uuid, char* p_buffer)
{
	// Check if the pointers are valid
	if (p_uuid == NULL) {
		return false;
	}

	// Clear the buffer
	memset(p_buffer, '\0', UUID_LENGTH_B);

	// Print into the buffer
	snprintf(p_buffer, FORMATTED_UUID_LENGTH_B, "%d-%d-%d-%d-%d-%d", *(p_uuid + 0), *(p_uuid + 1), *(p_uuid + 2), *(p_uuid + 3), *(p_uuid + 4), *(p_uuid + 5)); // NOLINT

	return true;
}

static void displayPrintConfigFile()
{
	// Open the file
	FILE* file = filesystemOpenFile(DISPLAY_CONFIG_NAME, "r", CONFIG_PARTITION);

	ESP_LOGI("main", "--- Content of 'displays_config.json' ---");
	char line[256];
	while (fgets(line, sizeof(line), (FILE*)file) != NULL) {
		esp_rom_printf("%s", line);
	}
	esp_rom_printf("\n");
	ESP_LOGI("main", "--- End of 'displays_config.json' ---");
}

/*
 *	Public function implementations
 */
DisplayConfig_t* displayRegister(const uint8_t* p_uuid)
{
	if (p_uuid == NULL) {
		return NULL;
	}

	// First check if the config file is loaded
	if (g_displayConfigFile.jsonRoot == NULL) {
		// Set the file path
		strlcpy(g_displayConfigFile.path, DISPLAY_CONFIG_NAME, MAX_CONFIG_FILE_PATH_LENGTH);

		// Load the config
		configLoad(&g_displayConfigFile);
	}

	// Create a config instance
	DisplayConfig_t config;

	// Copy the uuid
	memcpy(config.uuid, p_uuid, UUID_LENGTH_B);

	// Get its com id
	config.comId = getComId(p_uuid);

	/*
	 *	Display already known
	 */
	if (config.comId != 0) {
		// Get the entry
		for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
			if (g_displayRuntimeConfigs[i].comId == config.comId) {
				return &g_displayRuntimeConfigs[i];
			}
		}
		return NULL;
	}

	/*
	 *	Unknown display, so keep track of it
	*/
	return trackDisplay(&config);
}

void displaySetFirmwareVersion(const uint8_t comId, const uint8_t* p_firmware)
{
	if (comId == 0 || p_firmware == NULL) {
		return;
	}

	// Iterate through all display configs
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is it the correct display configuration?
		if (g_displayRuntimeConfigs[i].comId != comId) {
			continue;
		}

		// Set the firmware version to 0
		memset(g_displayRuntimeConfigs[i].firmwareVersion, '\0', FIRMWARE_LENGTH_B);

		// Is it a beta?
		g_displayRuntimeConfigs[i].firmwareVersion[0] = *p_firmware ? 'b' : ' ';

		// Copy the firmware version into the config
		snprintf(&g_displayRuntimeConfigs[i].firmwareVersion[1], FIRMWARE_LENGTH_B, "%d.%d.%d", *(p_firmware + 1),
		         *(p_firmware + 2), *(p_firmware + 3));

		return;
	}
}

void displaySetCommitInformation(const uint8_t comId, const uint8_t* p_commit)
{
	if (comId == 0 || p_commit == NULL) {
		return;
	}

	// Iterate through all display configs
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is it the correct display configuration?
		if (g_displayRuntimeConfigs[i].comId != comId) {
			continue;
		}

		// Set the commit to 0
		memset(g_displayRuntimeConfigs[i].commitHash, '\0', COMMIT_LENGTH_B);

		// Copy the firmware version into the config
		snprintf(&g_displayRuntimeConfigs[i].commitHash[0], COMMIT_LENGTH_B, "%c%c%c%c%c%c%c", *(p_commit),
		         *(p_commit + 1),
		         *(p_commit + 2), *(p_commit + 3), *(p_commit + 4), *(p_commit + 5), *(p_commit + 6));

		// Is it a dirty commit?
		g_displayRuntimeConfigs[i].commitHash[COMMIT_LENGTH_B - 1] = *(p_commit + 7) == true ? 'd' : ' ';

		return;
	}
}

void displayRestart(const uint8_t comId)
{
	// Do we have parameters?
	if (comId <= 0) {
		ESP_LOGD("DisplayManager", "Couldn't restart display. Received comID '0'");
		return;
	}

	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the com id
	frame.buffer[0] = comId;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_DISPLAY_RESTART, 1);

	// Send the frame
	canQueueFrame(&frame);
}

bool displayAllRegistered()
{
	return g_amountConnectedDisplays >= AMOUNT_OF_DISPLAYS;
}
