#pragma once

// C includes
#include <stdbool.h>
#include <stdio.h>

/*
 *	Public typedefs
 */
typedef enum
{
	CONFIG_PARTITION,
	DATA_PARTITION,
	SD_CARD,
} LOCATION_T;

/*
 *	Functions
 */
//! \brief Initializes the FileManager
//! \retval Boolean indicating if it was successful or not
bool fileManagerInit(void);

//! \brief Creates a new file at the specified location
//! \param path The path of the file including the file name and extension without "/sdcard/" or "/spiffs/"
//! \param location Where the file shall be created
//! \retval Boolean if the operation was successful
bool fileManagerCreateFile(const char* path, LOCATION_T location);

//! \brief Checks if a file at the specified path exists at the specified location
//! \param path The path to the file without "/sdcard/" or "/spiffs/"
//! \param location Where we should check for the file
//! \retval Boolean
bool fileManagerDoesFileExists(const char* path, const LOCATION_T location);

//! \brief Tries to open a file at the specified path and location
//! \param path The path to the file without "/sdcard/" or "/spiffs/"
//! \param mode The mode how the file should be opened
//! \param location Where the file is stored
//! \retval Returns a pointer to the FILE. Check for NULL!
FILE* fileManagerOpenFile(const char* path, const char* mode, const LOCATION_T location);

//! \brief Tries to delete a file at the specified path and location
//! \param path The path to the file without "/sdcard/" or "/spiffs/"
//! \param location Where the file is stored
//! \retval True if it was successful - False if not
bool fileManagerDeleteFile(const char* path, const LOCATION_T location);

//! \brief Checks if the specified directory exists on the SD Card
//! \param dir The path to the directory without "/sdcard/"
//! \retval True if the directory exists - False if it doesn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool fileManagerSDCardDoesDirectoryExist(const char* dir);

//! \brief Creates a directory at the specified path on the SD Card
//! \param path The path to the directory without "/sdcard/"
//! \retval True if it was successful - False if it wasn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool fileManagerSDCardCreateDir(const char* path);

//! \brief Deletes a directory at the specified path on the SD Card
//! \param path The path to the directory without "/sdcard/"
//! \retval True if it was successful - False if it wasn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool fileManagerSDCardDeleteDir(const char* path);

//! \brief Tests all functions of the FileManager system
void fileManager_test();
