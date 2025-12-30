#include "FileManager.h"

// Project includes
#include "logger.h"

// C includes
#include <sys/unistd.h>
#include <string.h>
#include <sys/stat.h>

// esp-idf includes
#include "driver/sdmmc_host.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"

/*
 *	Private defines
 */
//! \brief The GPIO for the clock
#define GPIO_CLK GPIO_NUM_16
//! \brief The GPIO for the command
#define GPIO_CMD GPIO_NUM_17
//! \brief The GPIO for the data0 line
#define GPIO_D0 GPIO_NUM_4
//! \brief The GPIO for the data1 line
#define GPIO_D1 GPIO_NUM_5
//! \brief The GPIO for the data2 line
#define GPIO_D2 GPIO_NUM_8
//! \brief The GPIO for the data3 line
#define GPIO_D3 GPIO_NUM_18

/*
 *	Private Variables
 */
static sdmmc_card_t* sdmmcCard_ = NULL;
static sdmmc_host_t sdmmcHost_ = SDMMC_HOST_DEFAULT();
static sdmmc_slot_config_t slotConfig_ = SDMMC_SLOT_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t mountConfig_;

// Is the SD Card mounted?
static bool sdCardMounted_ = false;

// Is the internal data partition mounted?
static bool dataPartitionMounted_ = false;

// Is the internal config partition mounted?
static bool configPartitionMounted_ = false;

/*
 *	Private Functions
 */
//! \brief Builds the whole path depending on the location
//! \param path The path that should be checked
//! \param location The location e.g. SD Card or Internal
//! \retval char* which contains the full path. Check for NULL!
static char* buildFullPath(const char* path, const int location)
{
	// Check the location
	if (location == CONFIG_PARTITION) {
		// Is the config partition mounted?
		if (!configPartitionMounted_) {
			// Logging
			loggerError("Config partition not mounted");

			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/config/";
		const uint8_t length = strlen(partition) + strlen(path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL) return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, path);
		return fullPath;
	}
	else if (location == DATA_PARTITION) {
		// Is the data partition mounted?
		if (!dataPartitionMounted_) {
			// Logging
			loggerError("Data partition not mounted");

			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/data/";
		const uint8_t length = strlen(partition) + strlen(path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL) return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, path);
		return fullPath;
	}
	else if (location == SD_CARD) {
		// Is the SD Card mounted?
		if (!sdCardMounted_) {
			// Logging
			loggerError("SD Card not mounted");

			// No so stop here!
			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/sdcard/";
		const uint8_t length = strlen(partition) + strlen(path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL) return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, path);
		return fullPath;
	}

	return NULL;
}

//! \brief Checks if the location is mounted
//! \param location The location which should be checked
static bool isLocationMounted(const LOCATION_T location)
{
	// Check if the location is mounted
	if (location == DATA_PARTITION && !dataPartitionMounted_) {
		return false;
	}
	if (location == CONFIG_PARTITION && !configPartitionMounted_) {
		return false;
	}
	if (location == SD_CARD && !sdCardMounted_) {
		return false;
	}
	return true;
}


/*
 *	Function implementations
 */
bool fileManagerInit(void)
{
	/*
	 * Initializing the micro SDCard slot
	 */

	// Initialize the SDMMC host
	sdmmcHost_.max_freq_khz = SDMMC_FREQ_HIGHSPEED; // 40 MHz
	sdmmcHost_.slot = SDMMC_HOST_SLOT_0;

	// Initialize the slot
	slotConfig_.width = 4;
	slotConfig_.clk = GPIO_CLK;
	slotConfig_.cmd = GPIO_CMD;
	slotConfig_.d0 = GPIO_D0;
	slotConfig_.d1 = GPIO_D1;
	slotConfig_.d2 = GPIO_D2;
	slotConfig_.d3 = GPIO_D3;

	// Initialize the mount config
	mountConfig_.format_if_mount_failed = false;
	mountConfig_.max_files = 5; // Max. amount of simultaneously opened files

	// Try to mount the SDCard
	const esp_err_t mountCardResult = esp_vfs_fat_sdmmc_mount("/sdcard", &sdmmcHost_, &slotConfig_, &mountConfig_,
	                                                          &sdmmcCard_);

	// Was the mount successful?
	if (mountCardResult == ESP_OK) {
		// The SDCard was mounted successfully
		sdCardMounted_ = true;

		// Logging
		loggerInfo("Mounted SD card successfully");
	}
	else {
		// Logging
		loggerError("Mounting SD card failed with error");
	}

	/*
	 * Initialize data partition
	 */

	// Initialize data config
	esp_vfs_spiffs_conf_t dataPartitionConfig;
	dataPartitionConfig.base_path = "/data";
	dataPartitionConfig.partition_label = "data";
	dataPartitionConfig.max_files = 5;
	dataPartitionConfig.format_if_mount_failed = false;

	// Register data partition
	const esp_err_t mountDataResult = esp_vfs_spiffs_register(&dataPartitionConfig);

	// Was the mount successful?
	if (mountDataResult == ESP_OK) {
		// The data partition was mounted successfully
		dataPartitionMounted_ = true;

		// Logging
		loggerInfo("Mounted data partition successfully");

		//esp_spiffs_format(NULL);
	}
	else {
		// Logging
		loggerError("Mounting data partition failed");
	}
	/*
	 * Initialize config partition
	 */
	// Initialize config config
	esp_vfs_spiffs_conf_t configPartitionConfig;
	configPartitionConfig.base_path = "/config";
	configPartitionConfig.partition_label = "config";
	configPartitionConfig.max_files = 5;
	configPartitionConfig.format_if_mount_failed = false;

	// Register config partition
	const esp_err_t mountConfigResult = esp_vfs_spiffs_register(&configPartitionConfig);

	// Was the mount successful?
	if (mountConfigResult == ESP_OK) {
		// The config partition was mounted successfully
		configPartitionMounted_ = true;

		// Logging
		loggerInfo("Mounted config partition successfully");

		//esp_spiffs_format(NULL);
	}
	else {
		// Logging
		loggerError("Mounting config partition failed");
	}


	// Return success
	return dataPartitionMounted_ && configPartitionMounted_
		&& sdCardMounted_;
}

bool fileManagerCreateFile(const char* path, const LOCATION_T location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, location);

	// For whatever reason fopen crashes with test.txt. I have absolutely no clue why but it costed me quite a few
	// hours until I found the crashes are caused by this :C
	if (strcmp(path, "test.txt") == 0) return false;

	// If the file does not yet exist
	if (!fileManagerDoesFileExists(path, location)) {
		// Try to create a file
		FILE* file = fopen(fullPath, "a+");

		// Was it successful?
		if (file == NULL) {
			// Logging
			loggerError("Failed creating file %s", fullPath);

			// Free the fullPath
			free(fullPath);

			// Then return NULL
			return false;
		}

		// Yes it was, now close it
		fclose(file);
	}

	// Yes, free the path and return true
	free(fullPath);
	return true;
}

bool fileManagerDoesFileExists(const char* path, const LOCATION_T location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, location);

	// Try to access that path
	const bool result = access(fullPath, F_OK); // 0 -> success ; 1 -> error

	// Free the full path
	free(fullPath);

	return !result;
}

FILE* fileManagerOpenFile(const char* path, const char* mode, const LOCATION_T location)
{
	// Check if the location is mounted
	if (!isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, location);
	if (fullPath == NULL) {
		return NULL;
	}

	// Try to open a file
	FILE* file = fopen(fullPath, mode);

	// Was it successful?
	if (file == NULL) {
		// Logging
		loggerError("Failed to open file. Path: %s ; Mode: %s", fullPath, mode);

		// Free the fullPath
		free(fullPath);

		// Then return NULL
		return NULL;
	}

	// Logging
	loggerDebug("Opened file %s", fullPath);

	// Yes, free the path and return the file
	free(fullPath);
	return file;
}

bool fileManagerDeleteFile(const char* path, const LOCATION_T location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, location);

	// Try to delete the file
	const bool result = !remove(fullPath); // 0 -> success ;  1 -> error
	if (result) {
		// Logging
		loggerInfo("Deleted file: %s", fullPath);
	}
	else {
		// Logging
		loggerWarn("Failed deleting file: %s", fullPath);
	}

	// Free the path
	free(fullPath);

	return result;
}

bool fileManagerDoesDirectoryExist(const char* dir)
{
	// Check if the sdcard is mounted
	if (isLocationMounted(SD_CARD)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(dir, SD_CARD);

	// Does the path exist?
	struct stat stats;
	stat(fullPath, &stats);
	const bool result = S_ISDIR(stats.st_mode);

	// Free fullPath
	free(fullPath);

	// Return result
	return result;
}

bool fileManagerCreateDir(const char* path)
{
	// Check if the sdcard is mounted
	if (!sdCardMounted_) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, SD_CARD);

	// Create the path
	const bool result = mkdir(fullPath, S_IRWXU) == 0 ? true : false; // 0 -> success ; -1 -> error

	// Did it fail?
	if (!result) {
		// Logging
		loggerWarn("Failed to create directory: %s", fullPath);
	}

	// Free fullPath
	free(fullPath);

	return result;
}

bool fileManagerDeleteDir(const char* path)
{
	// Check if the sdcard is mounted
	if (!sdCardMounted_) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(path, SD_CARD);

	// Create the path
	const bool result = rmdir(fullPath) == 0 ? true : false; // 0 -> success ; -1 -> error

	// Did it fail?
	if (!result) {
		// Logging
		loggerWarn("Failed to delete directory: %s", fullPath);
	}

	// Free fullPath
	free(fullPath);

	return result;
}

void fileManager_test()
{
	// Slots for two files
	FILE* file1 = NULL;
	FILE* file2 = NULL;
	FILE* file3 = NULL;

	// Create two files, one on each internal partition
	if (fileManagerCreateFile("hello.txt", CONFIG_PARTITION)) {
		file1 = fileManagerOpenFile("hello.txt", "a+", CONFIG_PARTITION);
	}
	if (fileManagerCreateFile("hello.txt", DATA_PARTITION)) {
		file2 = fileManagerOpenFile("hello.txt", "a+", DATA_PARTITION);
	}
	// And one on the SD Card
	if (fileManagerCreateFile("hello.txt", SD_CARD)) {
		file3 = fileManagerOpenFile("hello.txt", "a+", SD_CARD);
	}

	/* TESTING */

	// Close the files
	fclose(file1);
	fclose(file2);
	fclose(file3);

	// Delete the files
	fileManagerDeleteFile("hello.txt", CONFIG_PARTITION);
	fileManagerDeleteFile("hello.txt", DATA_PARTITION);
	fileManagerDeleteFile("hello.txt", SD_CARD);

	// Create a directory "location" on the SD Card
	if (fileManagerCreateDir("location")) {
		loggerInfo("The location on the SD Card was created successfully!");
	}

	// Check if these locations exists on the SD Card
	if (fileManagerDoesDirectoryExist("location")) {
		loggerInfo("The location exists on the SD Card!");
	}
	else {
		loggerWarn("The location does not exist on the SD Card!");
	}

	// Reset the file pointers
	file1 = NULL;
	file2 = NULL;

	// Create new file in the newly created directory
	if (fileManagerCreateFile("location/hello.txt", SD_CARD)) {
		file1 = fileManagerOpenFile("hello.txt", "a+", SD_CARD);
	}

	// Close the file again
	fclose(file1);

	// Then delete it
	fileManagerDeleteFile("location/hello.txt", SD_CARD);

	// Then delete the directory
	if (fileManagerDeleteDir("location")) {
		loggerInfo("Successfully deleted directory on SD Card!");
	}
	else {
		loggerWarn("Failed to delete the directory on the SD Card");
	}
}
