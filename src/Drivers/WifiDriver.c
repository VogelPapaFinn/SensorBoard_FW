#include "Drivers/WifiDriver.h"

// Project includes
#include "Config.h"
#include "EventQueues.h"

// C includes
#include <string.h>

// espidf includes
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>
#include <esp_sntp.h>
#include <esp_netif_sntp.h>

/*
 *	Private defines
 */
#define MAX_SSID_LENGTH 256
#define MAX_PSSWD_LENGTH 256

#define SNTP_TIMEOUT_MS 10000

/*
 *	Prototypes
 */
//! \brief Checks if the config file for the WiFi is loaded
//! \retval Bool indicating the status
bool isConfigFileLoaded();

//! \brief Loads the config file from the filesystem
//! \retval Bool indicating if the operation succeeded
bool loadConfigFile();

//! \brief Loads the SSID from the config file
//! \retval Bool indicating if the operation succeeded
bool loadSsid();

//! \brief Loads the Password from the config file
//! \retval Bool indicating if the operation succeeded
bool loadPsswd();

//! \brief Starts hosting an AP
//! \retval Bool indicating if the operation succeeded
bool startHostAp();

//! \brief Tries to join an AP
//! \retval Bool indicating if the operation succeeded
bool startJoinAp();

//! \brief Debug function to print the contents of the config file
static void wifiPrintConfigFile();

//! \brief Task used to synchronize the time via SNTP, when joining an AP
//! \param p_param Unused FreeRTOS parameter
static void synchronizeTimeTask(void* p_param);

/*
 *	Global variable
 */
uint8_t g_ipAddress[4] = { 0, 0, 0, 0 };

/*
 *	Private variables
 */
//! \brief The type of the WiFi e.g. hosting or joining an AP
static WifiType_t g_type = HOST_AP;

//! \brief The handle of the WiFi config
static ConfigFile_t g_config;

//! \brief String containing the WiFi SSID
static char g_ssid[MAX_SSID_LENGTH] = "";

//! \brief String containing the WiFi password
static char g_psswd[MAX_PSSWD_LENGTH] = "";

//! \brief Indicating if the WiFi was initialized (config loaded etc.)
static bool g_initialized = false;

//! \brief Indicating if something is active like a connection attempt
static bool g_active = false;

//! \brief Indicating if we are hosting or connected to an AP
static bool g_connected = false;

//! \brief Task handle for the time synchronization task
static TaskHandle_t g_synchronizeTimeTaskHandle = NULL;

/*
 *	ISRs, Event Handlers etc.
 */
//! \brief espidf WiFi event handler
static void wifiEventHandler(void* p_arg, const esp_event_base_t p_eventBase, const int32_t eventId, void* p_eventData)
{
	if (p_eventBase != WIFI_EVENT) {
		return;
	}

	// Connecting to WiFi AP
	if (eventId == WIFI_EVENT_STA_START) {
		ESP_LOGI("Wifi", "Connecting...");
		esp_wifi_connect();

		return;
	}

	// Disconnected from WiFi AP
	if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI("Wifi", "Disconnected, retrying...");

		g_connected = false;
		esp_wifi_connect();

		static uint8_t failCounter = 0;
		failCounter++;
		if (failCounter >= 10) {
			esp_wifi_disconnect();
			esp_wifi_stop();
			failCounter = 0;
			ESP_LOGI("Wifi", "Fail count reached. Aborting connection attempts.");
		}

		return;
	}
}

//! |brief espidf ip event handler
static void ipEventHandler(void* p_arg, const esp_event_base_t p_eventBase, const int32_t eventId, void* p_eventData)
{
	if (p_eventBase != IP_EVENT) {
		return;
	}

	// Successfully connected to WiFi AP
	if (p_eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
		const ip_event_got_ip_t* event = (ip_event_got_ip_t*)p_eventData;

		// Save the IP
		g_ipAddress[0] = esp_ip4_addr1_16(&event->ip_info.ip);
		g_ipAddress[1] = esp_ip4_addr2_16(&event->ip_info.ip);
		g_ipAddress[2] = esp_ip4_addr3_16(&event->ip_info.ip);
		g_ipAddress[3] = esp_ip4_addr4_16(&event->ip_info.ip);

		g_connected = true;

		ESP_LOGI("Wifi", "Got IP assigned: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));

		// Initialize the SNTP for time synchronization
		if (xTaskCreate(synchronizeTimeTask, "synchronizeTimeTask", 2048 * 4, NULL, 0, &g_synchronizeTimeTaskHandle) != pdPASS) {
			ESP_LOGE("Wifi", "Couldn't start time synchronization task!");
		}

		return;
	}
}

static void synchronizeTimeTask(void* p_param)
{
	ESP_LOGI("Wifi", "Initializing SNTP...");

	// Init
	const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
	esp_netif_sntp_init(&config);

	// Synchronize the time
	if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_TIMEOUT_MS)) != ESP_OK) {
		ESP_LOGW("Wifi", "Failed to synchronize system time. Wifi/HTTPS operations may fail because of this!");
	} else {
		// Set the time
		time_t now;
		time(&now);

		// Set timezone to Europe/Berlin
		setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
		tzset();

		// Logging
		char timeBuffer[64];
		struct tm timeinfo;
		localtime_r(&now, &timeinfo);
		strftime(timeBuffer, sizeof(timeBuffer), "%c", &timeinfo);
		ESP_LOGI("Wifi", "Synchronized system time: %s", timeBuffer);
	}

	// QueueEvent_t queueEvent;
	// queueEvent.command = WIFI_OTA_EXECUTE_UPDATE;
	// BaseType_t xHigherPriorityTaskWoken;
	// xQueueSendFromISR(g_operationManagerEventQueue, &queueEvent, &xHigherPriorityTaskWoken);

	vTaskDelete(NULL);
}

/*
 *	Private functions
 */
bool isConfigFileLoaded()
{
	return g_config.jsonRoot != NULL;
}

bool loadConfigFile()
{
	if (isConfigFileLoaded()) {
		return false;
	}

	// Set the path
	const char* path = "wifi_config.json";
	strlcpy(g_config.path, path, MAX_CONFIG_FILE_PATH_LENGTH);

	// Load the config
	return configLoad(&g_config);
}

bool loadSsid()
{
	// Build the names of the JSON entry
	char* ssidJsonName = "hostApSSID";
	if (g_type == JOIN_AP) {
		ssidJsonName = "joinApSSID";
	}

	// Check if the SSID in the config is valid
	const cJSON* ssid = cJSON_GetObjectItem(g_config.jsonRoot, ssidJsonName);
	if (ssid == NULL || !cJSON_IsString(ssid) || ssid->valuestring == NULL) {
		return false;
	}

	// Copy the SSID
	strlcpy(g_ssid, ssid->valuestring, MAX_SSID_LENGTH);

	return true;
}

bool loadPsswd()
{
	// Build the names of the JSON entry
	char* psswdJsonName = "hostApPassword";
	if (g_type == JOIN_AP) {
		psswdJsonName = "joinApPassword";
	}

	// Check if the PSSWD in the config is valid
	const cJSON* psswd = cJSON_GetObjectItem(g_config.jsonRoot, psswdJsonName);
	if (psswd == NULL || !cJSON_IsString(psswd) || psswd->valuestring == NULL) {
		return false;
	}

	// Copy the PSSWD
	strlcpy(g_psswd, psswd->valuestring, MAX_PSSWD_LENGTH);

	return true;
}

bool startHostAp()
{
	// Logging
	ESP_LOGI("Wifi", "Setting up AP...");

	// Create an AP instance
	esp_netif_create_default_wifi_ap();

	// Load the default config
	const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
	if (esp_wifi_init(&initConfig) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't initialize wifi config.");
		return false;
	}

	// Create the WiFi config
	wifi_config_t wifiConfig;
	wifiConfig = (wifi_config_t){
		.ap = {
			// .ssid = g_ssid,
			.ssid_len = strlen(g_ssid),
			.channel = 1,
			// .password = g_psswd,
			.max_connection = 4,
			.authmode = WIFI_AUTH_WPA2_PSK,
		}
	};
	strlcpy((char*)wifiConfig.ap.ssid, g_ssid, sizeof(wifiConfig.ap.ssid));
	strlcpy((char*)wifiConfig.ap.password, g_psswd, sizeof(wifiConfig.ap.password));

	// Start up the Wi-Fi AP
	if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't set wifi AP mode.");
		return false;
	}
	if (esp_wifi_set_config(WIFI_IF_AP, &wifiConfig) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't set wifi AP config.");
		return false;
	}
	if (esp_wifi_start() != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't start wifi AP.");
		return false;
	}

	// Set AP active
	g_active = true;

	return true;
}

bool startJoinAp()
{
	// Logging
	ESP_LOGI("Wifi", "Connecting to AP...");

	// Create an instance
	const esp_netif_t* netif = esp_netif_create_default_wifi_sta();

	// Set the name of the device
	esp_netif_set_hostname((esp_netif_t*)netif, "HybridDash");

	// Load the initial config
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	if (esp_wifi_init(&cfg) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't initialize wifi config.");
		return false;
	}

	// Create the WiFi config
	wifi_config_t wifiConfig;
	wifiConfig = (wifi_config_t){
		.sta =
		{
			//.ssid = joinApSSID_,
			//.password = joinApPassword_,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
		},
	};
	strlcpy((char*)wifiConfig.sta.ssid, g_ssid, sizeof(wifiConfig.sta.ssid));
	strlcpy((char*)wifiConfig.sta.password, g_psswd, sizeof(wifiConfig.sta.password));

	// Register the event handler
	if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL, NULL) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't register wifi handler instance for wifi events.");
		return false;
	}
	if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipEventHandler, NULL, NULL) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't register wifi handler instance for ip events.");
	}

	// Start up the Wi-Fi AP
	if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't set wifi AP mode.");
		return false;
	}
	if (esp_wifi_set_config(WIFI_IF_STA, &wifiConfig) != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't set wifi AP config.");
		return false;
	}
	if (esp_wifi_start() != ESP_OK) {
		ESP_LOGE("Wifi", "Couldn't start wifi AP.");
		return false;
	}

	// Set AP active
	g_active = true;

	return true;
}

#include "Drivers/FilesystemDriver.h"
static void wifiPrintConfigFile()
{
	// Open the file
	FILE* file = filesystemOpenFile("wifi_config.json", "r", CONFIG_PARTITION);

	ESP_LOGI("main", "--- Content of 'wifi_config.json' ---");
	char line[256];
	while (fgets(line, sizeof(line), (FILE*)file) != NULL) {
		esp_rom_printf("%s", line);
	}
	esp_rom_printf("\n");
	ESP_LOGI("main", "--- End of 'wifi_config.json' ---");
}

/*
 *	Public function implementations
 */
bool wifiSetType(const WifiType_t wifiType)
{
	g_type = wifiType;

	// Load the config
	loadConfigFile();

	// Error handling
	if (!isConfigFileLoaded()) {
		ESP_LOGE("Wifi", "Couldn't set wifi type. Config file could not be loaded.");
		return false;
	}

	// Check if we need to overwrite the WiFi type
	const cJSON* wifiMode = cJSON_GetObjectItem(g_config.jsonRoot, "wifiMode");
	if (wifiMode != NULL && cJSON_IsString(wifiMode) && wifiMode->valuestring != NULL) {
		// Yes check the type
		if (strcmp(wifiMode->valuestring, "JOIN_AP") == 0) {
			g_type = JOIN_AP;
		} else if (strcmp(wifiMode->valuestring, "HOST_AP") == 0) {
			g_type = HOST_AP;
		} else {
			ESP_LOGW("Wifi", "Recognized overwriting of WiFi mode but specified mode is invalid.");
		}
	}

	// Load the SSID
	if (!loadSsid()) {
		ESP_LOGE("Wifi", "Couldn't read SSID from config file.");
		return false;
	}

	// Load the PSSWD
	if (!loadPsswd()) {
		ESP_LOGE("Wifi", "Couldn't read PSSWD from config file.");
		return false;
	}

	return true;
}

WifiType_t wifiGetType()
{
	return g_type;
}

bool wifiConnect()
{
	if (g_active) {
		return false;
	}

	// Initialize everything needed if not yet done
	if (!g_initialized) {
		g_initialized = true;

		// Initialize NVS
		g_initialized &= nvs_flash_init() == ESP_OK;

		// Initialize TCP/IP
		g_initialized &= esp_netif_init() == ESP_OK;
		g_initialized &= esp_event_loop_create_default() == ESP_OK;

		// Did everything work?
		if (!g_initialized) {
			ESP_LOGE("Wifi", "Couldn't initialize WiFi events library.");

			return false;
		}
	}

	// Initialize the AP
	if (g_type == HOST_AP) {
		return startHostAp();
	}
	return startJoinAp();
}

bool wifiIsActive()
{
	return g_active;
}

bool wifiIsConnected()
{
	return g_connected;
}
