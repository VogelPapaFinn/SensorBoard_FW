#include "FilesystemDriver.h"

// C includes
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

// esp-idf includes
#include "driver/sdmmc_host.h"
#include "esp_log.h"
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
static sdmmc_card_t* g_sdmmcCard = NULL;
static sdmmc_host_t g_sdmmcHost = SDMMC_HOST_DEFAULT();
static sdmmc_slot_config_t g_slotConfig = SDMMC_SLOT_CONFIG_DEFAULT();
static esp_vfs_fat_sdmmc_mount_config_t g_mountConfig;

// Is the SD Card mounted?
static bool g_sdCardMounted = false;

// Is the internal data partition mounted?
static bool g_dataPartitionMounted = false;

// Is the internal config partition mounted?
static bool g_configPartitionMounted = false;

/*
 *	Private Functions
 */
//! \brief Builds the whole path depending on the location
//! \param path The path that should be checked
//! \param location The location e.g. SD Card or Internal
//! \retval char* which contains the full path. Check for NULL!
static char* buildFullPath(const char* p_path, const int location)
{
	// Check the location
	if (location == CONFIG_PARTITION) {
		// Is the config partition mounted?
		if (!g_configPartitionMounted) {
			// Logging
			ESP_LOGE("FilesystemDriver", "Config partition not mounted");

			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/config/";
		const uint8_t length = strlen(partition) + strlen(p_path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL)
			return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, p_path);
		return fullPath;
	}
	else if (location == DATA_PARTITION) {
		// Is the data partition mounted?
		if (!g_dataPartitionMounted) {
			// Logging
			ESP_LOGE("FilesystemDriver", "Data partition not mounted");

			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/data/";
		const uint8_t length = strlen(partition) + strlen(p_path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL)
			return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, p_path);
		return fullPath;
	}
	else if (location == SD_CARD) {
		// Is the SD Card mounted?
		if (!g_sdCardMounted) {
			// Logging
			ESP_LOGW("FilesystemDriver", "SD Card not mounted");

			// No so stop here!
			return NULL;
		}

		// Calculate the needed amount of memory and allocate it
		const char partition[] = "/sdcard/";
		const uint8_t length = strlen(partition) + strlen(p_path) + 1;
		char* fullPath = malloc(length);
		if (fullPath == NULL)
			return NULL;

		// Then build the full path and return it
		snprintf(fullPath, length, "%s%s", partition, p_path);
		return fullPath;
	}

	return NULL;
}

//! \brief Checks if the location is mounted
//! \param location The location which should be checked
static bool isLocationMounted(const Location_t location)
{
	// Check if the location is mounted
	if (location == DATA_PARTITION && !g_dataPartitionMounted) {
		return false;
	}
	if (location == CONFIG_PARTITION && !g_configPartitionMounted) {
		return false;
	}
	if (location == SD_CARD && !g_sdCardMounted) {
		return false;
	}
	return true;
}


/*
 *	Function implementations
 */
bool filesystemInit(void)
{
	/*
	 * Initializing the micro SDCard slot
	 */

	// Initialize the SDMMC host
	g_sdmmcHost.max_freq_khz = SDMMC_FREQ_HIGHSPEED; // 40 MHz
	g_sdmmcHost.slot = SDMMC_HOST_SLOT_0;

	// Initialize the slot
	g_slotConfig.width = 4;
	g_slotConfig.clk = GPIO_CLK;
	g_slotConfig.cmd = GPIO_CMD;
	g_slotConfig.d0 = GPIO_D0;
	g_slotConfig.d1 = GPIO_D1;
	g_slotConfig.d2 = GPIO_D2;
	g_slotConfig.d3 = GPIO_D3;

	// Initialize the mount config
	g_mountConfig.format_if_mount_failed = false;
	g_mountConfig.max_files = 5; // Max. amount of simultaneously opened files

	// Try to mount the SDCard
	const esp_err_t mountCardResult = esp_vfs_fat_sdmmc_mount("/sdcard", &g_sdmmcHost, &g_slotConfig, &g_mountConfig,
	                                                          &g_sdmmcCard);

	// Was the mount successful?
	if (mountCardResult == ESP_OK) {
		// The SDCard was mounted successfully
		g_sdCardMounted = true;

		// Logging
		ESP_LOGI("FilesystemDriver", "Mounted SD card successfully");
	}
	else {
		// Logging
		ESP_LOGW("FilesystemDriver", "Mounting SD card failed with error");
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
		g_dataPartitionMounted = true;

		// Logging
		ESP_LOGI("FilesystemDriver", "Mounted data partition successfully");

		//esp_spiffs_format(NULL);
	}
	else {
		// Logging
		ESP_LOGE("FilesystemDriver", "Mounting data partition failed");
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
		g_configPartitionMounted = true;

		// Logging
		ESP_LOGI("FilesystemDriver", "Mounted config partition successfully");

		//esp_spiffs_format(NULL);
	}
	else {
		// Logging
		ESP_LOGE("FilesystemDriver", "Mounting config partition failed");
	}


	// Return success
	return g_dataPartitionMounted && g_configPartitionMounted
		&& g_sdCardMounted;
}

bool filesystemCreateFile(const char* p_path, const Location_t location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, location);

	// For whatever reason fopen crashes with test.txt. I have absolutely no clue why but it costed me quite a few
	// hours until I found the crashes are caused by this :C
	if (strcmp(p_path, "test.txt") == 0)
		return false;

	// If the file does not yet exist
	if (!filesystemDoesFileExists(p_path, location)) {
		// Try to create a file
		FILE* file = fopen(fullPath, "a+");

		// Was it successful?
		if (file == NULL) {
			// Logging
			ESP_LOGE("FilesystemDriver", "Failed creating file %s", fullPath);

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

bool filesystemDoesFileExists(const char* p_path, const Location_t location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, location);

	// Try to access that path
	const bool result = access(fullPath, F_OK); // 0 -> success ; 1 -> error

	// Free the full path
	free(fullPath);

	return !result;
}

FILE* filesystemOpenFile(const char* p_path, const char* p_mode, const Location_t location)
{
	// Check if the location is mounted
	if (!isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, location);
	if (fullPath == NULL) {
		return NULL;
	}

	// Try to open a file
	FILE* file = fopen(fullPath, p_mode);

	// Was it successful?
	if (file == NULL) {
		// Logging
		ESP_LOGE("FilesystemDriver", "Failed to open file. Path: %s ; Mode: %s", fullPath, p_mode);

		// Free the fullPath
		free(fullPath);

		// Then return NULL
		return NULL;
	}

	// Logging
	ESP_LOGD("FilesystemDriver", "Opened file %s", fullPath);

	// Yes, free the path and return the file
	free(fullPath);
	return file;
}

bool filesystemDeleteFile(const char* p_path, const Location_t location)
{
	// Check if the location is mounted
	if (isLocationMounted(location)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, location);

	// Try to delete the file
	const bool result = !remove(fullPath); // 0 -> success ;  1 -> error
	if (result) {
		// Logging
		ESP_LOGI("FilesystemDriver", "Deleted file: %s", fullPath);
	}
	else {
		// Logging
		ESP_LOGW("FilesystemDriver", "Failed deleting file: %s", fullPath);
	}

	// Free the path
	free(fullPath);

	return result;
}

bool fileManagerDoesDirectoryExist(const char* p_dir)
{
	// Check if the sdcard is mounted
	if (isLocationMounted(SD_CARD)) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_dir, SD_CARD);

	// Does the path exist?
	struct stat stats;
	stat(fullPath, &stats);
	const bool result = S_ISDIR(stats.st_mode);

	// Free fullPath
	free(fullPath);

	// Return result
	return result;
}

bool fileManagerCreateDir(const char* p_path)
{
	// Check if the sdcard is mounted
	if (!g_sdCardMounted) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, SD_CARD);

	// Create the path
	const bool result = mkdir(fullPath, S_IRWXU) == 0 ? true : false; // 0 -> success ; -1 -> error

	// Did it fail?
	if (!result) {
		// Logging
		ESP_LOGW("FilesystemDriver", "Failed to create directory: %s", fullPath);
	}

	// Free fullPath
	free(fullPath);

	return result;
}

bool fileManagerDeleteDir(const char* p_path)
{
	// Check if the sdcard is mounted
	if (!g_sdCardMounted) {
		return false;
	}

	// Contains the full path to the file
	char* fullPath = buildFullPath(p_path, SD_CARD);

	// Create the path
	const bool result = rmdir(fullPath) == 0 ? true : false; // 0 -> success ; -1 -> error

	// Did it fail?
	if (!result) {
		// Logging
		ESP_LOGW("FilesystemDriver", "Failed to delete directory: %s", fullPath);
	}

	// Free fullPath
	free(fullPath);

	return result;
}

void filesystemTest()
{
	// Slots for two files
	FILE* file1 = NULL;
	FILE* file2 = NULL;
	FILE* file3 = NULL;

	// Create two files, one on each internal partition
	if (filesystemCreateFile("hello.txt", CONFIG_PARTITION)) {
		file1 = filesystemOpenFile("hello.txt", "a+", CONFIG_PARTITION);
	}
	if (filesystemCreateFile("hello.txt", DATA_PARTITION)) {
		file2 = filesystemOpenFile("hello.txt", "a+", DATA_PARTITION);
	}
	// And one on the SD Card
	if (filesystemCreateFile("hello.txt", SD_CARD)) {
		file3 = filesystemOpenFile("hello.txt", "a+", SD_CARD);
	}

	/* TESTING */

	// Close the files
	if (file1 != NULL) {
		fclose(file1);
	}
	if (file2 != NULL) {
		fclose(file2);
	}
	if (file3 != NULL) {
		fclose(file3);
	}

	// Delete the files
	filesystemDeleteFile("hello.txt", CONFIG_PARTITION);
	filesystemDeleteFile("hello.txt", DATA_PARTITION);
	filesystemDeleteFile("hello.txt", SD_CARD);

	// Create a directory "location" on the SD Card
	if (fileManagerCreateDir("location")) {
		ESP_LOGI("FilesystemDriver", "The location on the SD Card was created successfully!");
	}

	// Check if these locations exists on the SD Card
	if (fileManagerDoesDirectoryExist("location")) {
		ESP_LOGI("FilesystemDriver", "The location exists on the SD Card!");
	}
	else {
		ESP_LOGW("FilesystemDriver", "The location does not exist on the SD Card!");
	}

	// Reset the file pointers
	file1 = NULL;
	file2 = NULL;

	// Create new file in the newly created directory
	if (filesystemCreateFile("location/hello.txt", SD_CARD)) {
		file1 = filesystemOpenFile("hello.txt", "a+", SD_CARD);
	}

	// Close the file again
	if (file1 != NULL) {
		fclose(file1);
	}

	// Then delete it
	filesystemDeleteFile("location/hello.txt", SD_CARD);

	// Then delete the directory
	if (fileManagerDeleteDir("location")) {
		ESP_LOGI("FilesystemDriver", "Successfully deleted directory on SD Card!");
	}
	else {
		ESP_LOGW("FilesystemDriver", "Failed to delete the directory on the SD Card");
	}
}
