#include "DisplayManager.h"

// Project includes
#include "ConfigManager.h"
#include "can.h"
#include "FilesystemDriver.h"

// espidf includes
#include <esp_log.h>
#include <esp_timer.h>

/*
 *	Private Defines
 */
#define AMOUNT_OF_DISPLAYS 1
#define UUID_LENGTH_B 6
#define FORMATTED_UUID_LENGTH_B 24
#define DISPLAY_CONFIG_NAME "displays_config.json"
#define REGISTRATION_REQUEST_INTERVAL_MICROS (1000 * 1000) // 1000 milliseconds

#define FIRMWARE_LENGTH 5
#define HASH_LENGTH 9

/*
 *	Private typedefs
 */
//! \brief A struct representing an uuid which got a comId assigned
typedef struct
{
	//! \brief The uuid of the display. Has the length of UUID_LENGTH_B
	char uuid[sizeof(uint8_t) * UUID_LENGTH_B];

	//! \brief The assigned comId. 0 if this entry is faulty/unused
	uint8_t comId;

	//! \brief The firmware version of the display
	char* firmwareVersion;

	//! \brief The commit hash of the firmware version
	char* commitHash;
} DisplayConfig_t;

/*
 *	Private function prototypes
 */
//! \brief Broadcasts the registration requests on the CAN bus
//! \param p_arg Unused pointer which is needed for the espidf timer to trigger the function correctly
static void broadcastRegistrationRequestCb(void* p_arg);

//! \brief Returns the comId assigned to the specified uuid
//! \param p_uuid The uuid array, usually 6 Bytes long
//! \retval The comId or 0 if none was found
static uint8_t getComIdFromUuid(const uint8_t* p_uuid);

//! \brief Generates a new com id for the specified uuid
//! \param p_uuid The uuid array, usually 6 Bytes long
//! \retval The comId or 0 if none could be generated
static uint8_t createConfigForUnknownDevice(const uint8_t* p_uuid);

//! \brief Checks if we have an entry in the config file for the specified uuid
//! \param p_uuid The uuid array of the display. Usually 6 Bytes long
//! \retval A bool indicating if an entry exists or not
static bool checkConfigForUuidInConfigFileExists(const uint8_t* p_uuid);

//! \brief Loads the to be displayed screen for the specified uuid from the config file
//! \param p_uuid The uuid array of the display. Usually 6 Bytes long
//! \retval A Screen_t instance but converted to an uint8_t
static uint8_t loadScreenForComIdFromFile(const uint8_t* p_uuid);

//! \brief Formats a specified uuid for better displaying and handling in the config file
//! \param p_uuid The uuid array, usually 6 Bytes long
//! \param p_buffer The output buffer, where the formatted uuid should be placed into
//! \param bufferLength The length of the passed buffer
//! \retval A bool indicating if the write process was successfull
static bool getFormattedUuid(const uint8_t* p_uuid, char* p_buffer, uint8_t bufferLength);

//! \brief Debug function to print the content of the configuration file
static void displayPrintConfigFile();

/*
 *	Private Variables
 */
//! \brief Amount of connected displays. Used for assigning the COM IDs
static uint8_t g_amountOfConnectedDisplays;

//! \brief Bool indicating if the registration process is currently active
static bool g_registrationProcessActive = false;

//! \brief Handle of the timer which is used in the registration process to periodically
//! send the registration request
static esp_timer_handle_t g_registrationTimerHandle;

//! \brief The config of the timer which is used in the registration process to periodically
//! send the registration request
static const esp_timer_create_args_t g_registrationTimerConfig = {.callback = &broadcastRegistrationRequestCb,
                                                                  .name = "Display Registration Timer"};

//! \brief The array which holds the DisplayConfig_t instances
static DisplayConfig_t g_displayConfigs[AMOUNT_OF_DISPLAYS];

/*
 *	Private functions
 */
static void broadcastRegistrationRequestCb(void* p_arg)
{
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_REGISTRATION, g_ownCanComId, 0);

	// Send the frame
	canQueueFrame(&frame);
}

static uint8_t getComIdFromUuid(const uint8_t* p_uuid)
{
	// Is the uuid valid?
	if (p_uuid == NULL) {
		return 0;
	}

	// Extract the formatted UUID
	char formattedUuid[FORMATTED_UUID_LENGTH_B];
	getFormattedUuid(p_uuid, formattedUuid, sizeof(formattedUuid));

	// Iterate through the list of all com id entries
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is the entry not empty?
		if (g_displayConfigs[i].comId != 0) {
			// Extract the formatted UUID for the entry
			char formattedUuidEntry[FORMATTED_UUID_LENGTH_B];
			if (!getFormattedUuid((uint8_t*)g_displayConfigs[i].uuid, formattedUuidEntry, sizeof(formattedUuidEntry))) {
				ESP_LOGE("DisplayManager", "Couldn't get the formatted uuid entry string");
				return 0;
			}

			// Check if the uuids match
			ESP_LOGI("DisplayManager", "%s %s", formattedUuid, formattedUuidEntry);
			if (strcmp(formattedUuid, formattedUuidEntry) == 0) {
				// Return com id
				return g_displayConfigs[i].comId;
			}
		}
	}

	// Nothing found
	return 0;
}

static uint8_t createConfigForUnknownDevice(const uint8_t* p_uuid)
{
	uint8_t comId = 0;

	// Is the uuid valid?
	if (p_uuid == NULL) {
		return 0;
	}

	/*
	 *	Find an empty com id entry in the array we can use
	 */
	// Iterate through the list of all com id entries
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Find the first empty entry
		if (g_displayConfigs[i].comId == 0) {
			// Set the UUID
			g_displayConfigs[i].uuid[0] = *(p_uuid + 0); // NOLINT
			g_displayConfigs[i].uuid[1] = *(p_uuid + 1); // NOLINT
			g_displayConfigs[i].uuid[2] = *(p_uuid + 2); // NOLINT
			g_displayConfigs[i].uuid[3] = *(p_uuid + 3); // NOLINT
			g_displayConfigs[i].uuid[4] = *(p_uuid + 4); // NOLINT
			g_displayConfigs[i].uuid[5] = *(p_uuid + 5); // NOLINT

			// Set the com id
			g_displayConfigs[i].comId = ++g_amountOfConnectedDisplays;

			// Save the com id
			comId = g_displayConfigs[i].comId;
			break;
		}
	}

	// Did we find an empty entry?
	if (comId == 0) {
		return 0;
	}

	/*
	 *	Check if we already have an entry in the config file with the same uuid
	 */
	const bool entryExists = checkConfigForUuidInConfigFileExists(p_uuid);
	if (entryExists) {
		// Set the screen
		return comId;
	}

	/*
	 *	Load the config file
	*/
	// Extract the formatted UUID
	char formattedUuid[FORMATTED_UUID_LENGTH_B];
	getFormattedUuid(p_uuid, formattedUuid, sizeof(formattedUuid));

	// Get the cJSON root object
	cJSON* root = *configGet(DISPLAY_CONFIG);

	// Is it valid?
	if (root == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty config for %d", DISPLAY_CONFIG);
		return comId;
	}

	// Get the display configurations
	cJSON* displayConfigurationsArray = cJSON_GetObjectItem(root, "displayConfigurations");

	// Are they valid
	if (displayConfigurationsArray == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty display configurations from %d", DISPLAY_CONFIG);
		return comId;
	}

	/*
	 *	Create a new entry in the config file
	 */
	// Create a new entry
	cJSON* newConfigurationEntry = cJSON_CreateObject();
	cJSON_AddStringToObject(newConfigurationEntry, "hwUuid", formattedUuid);
	cJSON_AddStringToObject(newConfigurationEntry, "screen", "temperature");

	// Add the entry to the array
	cJSON_AddItemToArray(displayConfigurationsArray, newConfigurationEntry);

	// Then save the new config
	if (!configWriteToFile(DISPLAY_CONFIG)) {
		ESP_LOGE("DisplayManager", "Couldn't write new display configuration to file");
	}
	else {
		ESP_LOGI("DisplayManager", "Written new display configuration to file");
	}

	return comId;
}

static bool checkConfigForUuidInConfigFileExists(const uint8_t* p_uuid)
{
	// Is the uuid valid?
	if (p_uuid == NULL) {
		return SCREEN_UNKNOWN;
	}

	// Get the cJSON root object
	const cJSON* root = *configGet(DISPLAY_CONFIG);

	// Is it valid?
	if (root == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty config for %d", DISPLAY_CONFIG);
		return SCREEN_UNKNOWN;
	}

	// Get the display configurations
	const cJSON* displayConfigurationsArray = cJSON_GetObjectItem(root, "displayConfigurations");

	// Are they valid
	if (displayConfigurationsArray == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty display configurations from %d", DISPLAY_CONFIG);
		return SCREEN_UNKNOWN;
	}

	// Check all config entries
	for (uint8_t i = 0; i < (uint8_t)cJSON_GetArraySize(displayConfigurationsArray); i++) {
		// Get the current entry
		const cJSON* configuration = cJSON_GetArrayItem(displayConfigurationsArray, i);

		// Get the UUID
		const cJSON* uuid = cJSON_GetObjectItem(configuration, "hwUuid");
		if (!cJSON_IsString(uuid) || (uuid->valuestring == NULL)) {
			continue;
		}

		// Extract the formatted UUID
		char formattedUuid[FORMATTED_UUID_LENGTH_B];
		getFormattedUuid(p_uuid, formattedUuid, sizeof(formattedUuid));

		// Is it the same UUID?
		if (strcmp(uuid->valuestring, formattedUuid) != 0) {
			continue;
		}

		return true;
	}

	// Nothing found
	return false;
}

static uint8_t loadScreenForComIdFromFile(const uint8_t* p_uuid)
{
	// Is the uuid valid?
	if (p_uuid == NULL) {
		return SCREEN_UNKNOWN;
	}

	// Get the cJSON root object
	const cJSON* root = *configGet(DISPLAY_CONFIG);

	// Is it valid?
	if (root == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty config for %d", DISPLAY_CONFIG);
		return SCREEN_UNKNOWN;
	}

	// Get the display configurations
	const cJSON* displayConfigurationsArray = cJSON_GetObjectItem(root, "displayConfigurations");

	// Are they valid
	if (displayConfigurationsArray == NULL) {
		ESP_LOGE("DisplayManager", "Got faulty display configurations from %d", DISPLAY_CONFIG);
		return SCREEN_UNKNOWN;
	}

	// Check all config entries
	for (uint8_t i = 0; i < (uint8_t)cJSON_GetArraySize(displayConfigurationsArray); i++) {
		// Get the current entry
		const cJSON* configuration = cJSON_GetArrayItem(displayConfigurationsArray, i);

		// Get the UUID
		const cJSON* uuid = cJSON_GetObjectItem(configuration, "hwUuid");
		if (!cJSON_IsString(uuid) || (uuid->valuestring == NULL)) {
			continue;
		}

		// Extract the formatted UUID
		char formattedUuid[FORMATTED_UUID_LENGTH_B];
		getFormattedUuid(p_uuid, formattedUuid, sizeof(formattedUuid));

		// Is it the same UUID?
		if (strcmp(uuid->valuestring, formattedUuid) != 0) {
			continue;
		}

		// Get the screen from the config and check if its valid
		const cJSON* screen = cJSON_GetObjectItem(configuration, "screen");
		if (!cJSON_IsString(screen) || (screen->valuestring == NULL)) {
			continue;
		}

		// Then return the screen
		if (strcmp(screen->valuestring, "temperature") == 0) {
			return SCREEN_TEMPERATURE;
		}
		if (strcmp(screen->valuestring, "speed") == 0) {
			return SCREEN_SPEED;
		}
		if (strcmp(screen->valuestring, "rpm") == 0) {
			return SCREEN_RPM;
		}
		return SCREEN_UNKNOWN;
	}

	// No match
	return SCREEN_UNKNOWN;
}

static bool getFormattedUuid(const uint8_t* p_uuid, char* p_buffer, const uint8_t bufferLength)
{
	// Check if the pointers are valid
	if (p_uuid == NULL || p_buffer == NULL) {
		return false;
	}

	// Print into the buffer
	snprintf(p_buffer, bufferLength, "%d-%d-%d-%d-%d-%d", *(p_uuid + 0), *(p_uuid + 1), *(p_uuid + 2), *(p_uuid + 3), *(p_uuid + 4), *(p_uuid + 5)); // NOLINT

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
void displayManagerInit()
{
	// Load the display config
	configLoadFile(&DISPLAY_CONFIG_NAME[0], DISPLAY_CONFIG);

	// Print the content of the config file
	displayPrintConfigFile();
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
	canInitiateFrame(&frame, CAN_MSG_DISPLAY_RESTART, g_ownCanComId, 1);

	// Send the frame
	canQueueFrame(&frame);
}

void displayStartRegistrationProcess()
{
	// Are we already in the registration process?
	if (g_registrationProcessActive) {
		ESP_LOGW("DisplayManager", "There were multiple attempts to start the display registration process!");
		return;
	}

	// Start the process
	g_registrationProcessActive = true;

	// Create the registration timer
	esp_timer_create(&g_registrationTimerConfig, &g_registrationTimerHandle);

	// Then start the timer
	esp_timer_start_periodic(g_registrationTimerHandle, (uint64_t)REGISTRATION_REQUEST_INTERVAL_MICROS);
}

uint8_t displayRegisterWithUUID(const uint8_t* p_uuid)
{
	// Check if uuid is valid
	if (p_uuid == NULL) {
		ESP_LOGE("DisplayManager", "Received a NULL ID in the registration process");
		return 0;
	}

	/*
	 *	Check if we do we already have enough displays
	 */
	uint8_t comId = 0;
	if (g_amountOfConnectedDisplays >= AMOUNT_OF_DISPLAYS) {
		// Get its com id
		comId = getComIdFromUuid(p_uuid);

		// Is it a valid com id?
		if (comId == 0) {
			ESP_LOGW("DisplayManager", "A device tried to register itself but we already know %d devices",
			         AMOUNT_OF_DISPLAYS);
			return 0;
		}
	}

	/*
	 *	Create a new configuration if it is an unknown device
	 */
	if (comId == 0) {
		comId = createConfigForUnknownDevice(p_uuid);

		// Couldn't create com id
		if (comId == 0) {
			ESP_LOGE("DisplayManager", "Couldn't create com id for newly registered device");
			return 0;
		}
	}

	/*
	 *	Get the screen
	 */
	Screen_t screen = loadScreenForComIdFromFile(p_uuid);

	// If it is unknown, replace it with the default screen
	if (screen == SCREEN_UNKNOWN) {
		screen = SCREEN_TEMPERATURE;
	}

	/*
	 *	Create and send the CAN message
	 */
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Insert the UUID
	frame.buffer[0] = *(p_uuid + 0); // NOLINT
	frame.buffer[1] = *(p_uuid + 1); // NOLINT
	frame.buffer[2] = *(p_uuid + 2); // NOLINT
	frame.buffer[3] = *(p_uuid + 3); // NOLINT
	frame.buffer[4] = *(p_uuid + 4); // NOLINT
	frame.buffer[5] = *(p_uuid + 5); // NOLINT

	// Insert the com id
	frame.buffer[6] = comId; // NOLINT

	// Insert the screen
	frame.buffer[7] = screen; // NOLINT

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_COMID_ASSIGNATION, g_ownCanComId, 8);

	// Send the frame
	canQueueFrame(&frame);

	// Logging
	ESP_LOGI("DisplayManager", "Sending ID '%d' and screen '%d' to UUID '%d-%d-%d-%d-%d-%d'", frame.buffer[6],
	         frame.buffer[7], frame.buffer[0],
	         frame.buffer[1],
	         frame.buffer[2], frame.buffer[3], frame.buffer[4], frame.buffer[5]);

	// Check if all devices registered
	if (g_registrationProcessActive && g_amountOfConnectedDisplays >= AMOUNT_OF_DISPLAYS) {
		// Stop and delete the registration timer
		esp_timer_stop(g_registrationTimerHandle);

		// Enter the operation mode
		QueueEvent_t event;
		event.command = INIT_OPERATION_MODE;
		xQueueSend(g_mainEventQueue, &event, pdMS_TO_TICKS(100));
	}

	return comId;
}

void displaySetFirmwareVersion(const uint8_t comId, const uint8_t* p_firmware)
{
	if (p_firmware == NULL) {
		return;
	}

	DisplayConfig_t* config = NULL;

	// Iterate through all display configs
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is it the correct display configuration?
		if (g_displayConfigs[i].comId == comId) {
			config = &g_displayConfigs[i];

			break;
		}
	}

	// Did we find a config?
	if (config == NULL) {
		ESP_LOGE("DisplayManager", "Couldn't find a display config for comId %d", comId);

		return;
	}

	// Allocate needed memory
	config->firmwareVersion = malloc(FIRMWARE_LENGTH);
	if (config->firmwareVersion == NULL) {
		ESP_LOGE("DisplayManager", "Couldn't allocate memory for the firmware stringf of comId %d", comId);

		return;
	}
	memset(config->firmwareVersion, ' ', FIRMWARE_LENGTH);
	config->firmwareVersion[FIRMWARE_LENGTH - 1] = '\0';

	// Is it a beta version?
	if (*p_firmware == true) {
		config->firmwareVersion[0] = 'b';
	}

	// Get the major version
	config->firmwareVersion[1] = (char)*++p_firmware;

	// Get the minor version
	config->firmwareVersion[2] = (char)*++p_firmware;

	// Get the patch version
	config->firmwareVersion[3] = (char)*++p_firmware;

	// Logging
	ESP_LOGI("DisplayManager", "Received firmware version: %c%c.%c.%c for com id: %d", config->firmwareVersion[0],
	         config->firmwareVersion[1], config->firmwareVersion[2], config->firmwareVersion[3], comId);
}

void displaySetCommitInformation(const uint8_t comId, const uint8_t* p_information)
{
	if (p_information == NULL) {
		return;
	}

	// Iterate through all display configs
	DisplayConfig_t* config = NULL;
	for (uint8_t i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
		// Is it the correct display configuration?
		if (g_displayConfigs[i].comId == comId) {
			config = &g_displayConfigs[i];

			break;
		}
	}

	// Did we find a config?
	if (config == NULL) {
		ESP_LOGE("DisplayManager", "Couldn't find a display config for comId %d", comId);

		return;
	}

	// Allocate needed memory
	config->commitHash = malloc(HASH_LENGTH);
	if (config->commitHash == NULL) {
		ESP_LOGE("DisplayManager", "Couldn't allocate memory for the firmware string of comId %d", comId);

		return;
	}
	memset(config->commitHash, ' ', HASH_LENGTH);
	config->commitHash[HASH_LENGTH - 1] = '\0';

	// Get all chars
	for (uint8_t i = 0; i < HASH_LENGTH - 1; i++) {
		config->commitHash[i] = (char)*(p_information + i);
	}

	// Is it dirty version?
	if (*(p_information + 7) == true) {
		config->commitHash[HASH_LENGTH - 2] = 'd';
	}
}
