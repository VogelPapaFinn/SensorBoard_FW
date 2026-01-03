#pragma once

// C includes
#include <stdbool.h>
#include <stdio.h>

/*
 *	Public typedefs
 */
//! \brief Typedef Enum indicating a location/partition on the filesystem
typedef enum
{
	CONFIG_PARTITION,
	DATA_PARTITION,
	SD_CARD,
} Location_t;

/*
 *	Functions
 */
//! \brief Initializes the FileManager
//! \retval Boolean indicating if it was successful or not
bool filesystemInit(void);

//! \brief Creates a new file at the specified location
//! \param p_path The path of the file including the file name and extension without "/sdcard/" or "/spiffs/"
//! \param location Where the file shall be created
//! \retval Boolean if the operation was successful
bool filesystemCreateFile(const char* p_path, Location_t location);

//! \brief Checks if a file at the specified path exists at the specified location
//! \param p_path The path to the file without "/sdcard/" or "/spiffs/"
//! \param location Where we should check for the file
//! \retval Boolean
bool filesystemDoesFileExists(const char* p_path, Location_t location);

//! \brief Tries to open a file at the specified path and location
//! \param p_path The path to the file without "/sdcard/" or "/spiffs/"
//! \param p_mode The mode how the file should be opened
//! \param location Where the file is stored
//! \retval Returns a pointer to the FILE. Check for NULL!
FILE* filesystemOpenFile(const char* p_path, const char* p_mode, Location_t location);

//! \brief Tries to delete a file at the specified path and location
//! \param p_path The path to the file without "/sdcard/" or "/spiffs/"
//! \param location Where the file is stored
//! \retval True if it was successful - False if not
bool filesystemDeleteFile(const char* p_path, Location_t location);

//! \brief Checks if the specified directory exists on the SD Card
//! \param p_dir The path to the directory without "/sdcard/"
//! \retval True if the directory exists - False if it doesn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool filesystemSDCardDoesDirectoryExist(const char* p_dir);

//! \brief Creates a directory at the specified path on the SD Card
//! \param p_path The path to the directory without "/sdcard/"
//! \retval True if it was successful - False if it wasn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool filesystemSDCardCreateDir(const char* p_path);

//! \brief Deletes a directory at the specified path on the SD Card
//! \param p_path The path to the directory without "/sdcard/"
//! \retval True if it was successful - False if it wasn't
//! \note The SPIFFS filesystem unfortunately does not support directories
bool filesystemSDCardDeleteDir(const char* p_path);

//! \brief Tests all functions of the FileManager system
void filesystemTest();
