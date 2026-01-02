// Project includes
#include "ConfigManager.h"
#include "DataCenter.h"
#include "FileManager.h"
#include "logger.h"
#include "WebInterface.h"

// espidf includes
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>
#include <sys/dirent.h>
#include <sys/param.h>
#include "../../esp-idf/components/json/cJSON/cJSON.h"

/*
 *	Defines
 */
#define FILE_UPLOAD_BUFFER_SIZE_B 1024
#define MAX_HTTP_CONNECTIONS 255

/*
 *	Prototypes
 */
static void wifiEventHandler(void* p_arg, esp_event_base_t p_eventBase, int32_t eventID, void* p_eventData);
static const char* getMimeType(const char* p_filepath);
static esp_err_t requestHandler(httpd_req_t* p_reqst);
static esp_err_t fileHandler(httpd_req_t* p_reqst);

/*
 *	Other Handlers
 */

/*
 *	URIs
 */
static const httpd_uri_t g_webinterfaceHandlerURI = {
	.uri = "/*", .method = HTTP_GET, .handler = fileHandler, .user_ctx = NULL
};
static const httpd_uri_t g_webinterfaceWebsocketHandlerURI = {
	.uri = "/ws*", .method = HTTP_GET, .handler = requestHandler, .user_ctx = NULL, .is_websocket = true
};
static const httpd_uri_t g_webinterfaceApiGETHandlerURI = {
	.uri = "/api/get*", .method = HTTP_GET, .handler = requestHandler, .user_ctx = NULL
};
static const httpd_uri_t g_webinterfaceApiPOSTHandlerURI = {
	.uri = "/api/post*", .method = HTTP_POST, .handler = requestHandler, .user_ctx = NULL
};

/*
 *	Private Variables
 */
httpd_handle_t g_httpdHandle = NULL;
bool g_websocketConnected = false;

char* g_joinApSSID = NULL;
char* g_joinApPassword = NULL;
char* g_hostApSSID = NULL;
char* g_hostApPassword = NULL;

/*
 *	File Handler
 */
static const char* getMimeType(const char* p_filepath)
{
	const char* lastDot = strrchr(p_filepath, '.');
	if (!lastDot)
		return "text/plain";
	if (strcmp(lastDot, ".html") == 0)
		return "text/html";
	if (strcmp(lastDot, ".css") == 0)
		return "text/css";
	if (strcmp(lastDot, ".js") == 0)
		return "application/javascript";
	if (strcmp(lastDot, ".png") == 0)
		return "image/png";
	if (strcmp(lastDot, ".jpg") == 0)
		return "image/jpeg";
	if (strcmp(lastDot, ".ico") == 0)
		return "image/x-icon";
	return "text/plain";
}

static esp_err_t requestHandler(httpd_req_t* p_reqst)
{
	// Websocket Handshake
	if (strcmp(p_reqst->uri, "/ws") == 0) {
		// Handshake
		if (p_reqst->method == HTTP_GET) {
			g_websocketConnected = true;
			return ESP_OK;
		}
	}
	// Request of initial data
	else if (strcmp(p_reqst->uri, "/api/get/initial_data") == 0) {
		// Send the initial data
		const char* const jsonOutput = getAllDisplayStatiAsJSON();
		if (jsonOutput != NULL) {
			webinterfaceSendData();
		}
		return ESP_OK;
	}
	else if (strcmp(p_reqst->uri, "/api/post/restart_display") == 0) {
		// Is it a POST request?
		if (p_reqst->method != HTTP_POST) {
			return ESP_ERR_INVALID_ARG;
		}

		// Get the data
		char dataBuffer[128];
		const uint8_t dataLength = p_reqst->content_len;
		httpd_req_recv(p_reqst, dataBuffer, sizeof(dataBuffer));

		// Parse it to JSON
		cJSON* root = cJSON_Parse(dataBuffer);
		if (root == NULL) {
			return ESP_ERR_INVALID_ARG;
		}

		// Get the COMID
		const cJSON* id = cJSON_GetObjectItem(root, "id");
		if (cJSON_IsString(id) && (id->valuestring != NULL)) {
			// Restart the display
			QueueEvent_t restartDisplayRequest;
			restartDisplayRequest.command = QUEUE_RESTART_DISPLAY;

			// Allocate the parameter memory and copy the value to the memory
			restartDisplayRequest.parameter = malloc(strlen(id->valuestring));
			memcpy(restartDisplayRequest.parameter, id->valuestring, strlen(id->valuestring));

			// Set the parameter length
			restartDisplayRequest.parameterLength = strlen(id->valuestring);

			// Send it to the queue
			xQueueSend(g_mainEventQueue, &restartDisplayRequest, portMAX_DELAY);
		}

		// Free memory
		cJSON_Delete(root);
	}
	return ESP_OK;
}

static esp_err_t fileHandler(httpd_req_t* p_reqst)
{
	// Array which holds the file path of the file we return
	char filepath[600];

	// Check if it is the index.html file
	if (strcmp(p_reqst->uri, "") == 0) {
		return ESP_ERR_INVALID_ARG;
	}
	// File request
	if (strcmp(p_reqst->uri, "/") == 0) {
		strcpy(filepath, "webinterface/index.html");
	}
	else {
		// Otherwise build the absolute path to the requested file
		snprintf(filepath, sizeof(filepath), "webinterface%s", p_reqst->uri);
	}

	// Then open the file as read only
	FILE* reqFile = fileManagerOpenFile(filepath, "r", DATA_PARTITION);
	if (reqFile == NULL) {
		// Error handling
		esp_rom_printf("Couldn't open file: %s\n", filepath);
		return ESP_FAIL;
	}

	// Set the MIME Type
	httpd_resp_set_type(p_reqst, getMimeType(filepath));

	// Send the file in 4KB chunks
	char* chunk = malloc(4096);
	if (!chunk) {
		// Close the file
		fclose(reqFile);
		return ESP_ERR_NO_MEM;
	}

	// Read all chunks and send them to the requestor
	size_t chunksize;
	do {
		// Read 4KB of data from the requested file
		chunksize = fread(chunk, 1, 4096, reqFile);

		// Send if the chunk is not emptz
		if (chunksize > 0) {
			// Send the chunk to the requestor
			if (httpd_resp_send_chunk(p_reqst, chunk, (ssize_t)chunksize) != ESP_OK) {
				// Something went wrong
				fclose(reqFile);
				free(chunk);
				return ESP_FAIL;
			}
		}
	}
	while (chunksize != 0); // Do until the chunksize is 0 -> file was completely read

	// Cleanup
	fclose(reqFile);
	free(chunk);

	// Send one last empty chunk to signalize the end of the transmission
	httpd_resp_send_chunk(p_reqst, NULL, 0);

	return ESP_OK;
}

/*
 *	Function implementations
 */

void webinterfaceBroadcastWorker(void* p_arg)
{
	// Cast the arg to a message
	char* message = p_arg;
	if (message == NULL) {
		return;
	}

	// Get all open connections
	size_t amountOfOpenConnections = MAX_HTTP_CONNECTIONS;
	int connectionFDS[MAX_HTTP_CONNECTIONS];

	// Pull all FDS
	if (httpd_get_client_list(g_httpdHandle, &amountOfOpenConnections, connectionFDS) == ESP_OK) {
		// Create the packet
		httpd_ws_frame_t packet;
		packet.payload = (uint8_t*)message;
		packet.len = strlen(message);
		packet.type = HTTPD_WS_TYPE_TEXT;

		// Send it to all open connections
		for (int i = 0; i < amountOfOpenConnections; i++) {
			httpd_ws_send_frame_async(g_httpdHandle, connectionFDS[i], &packet);
		}
	}

	// Finally free the message memory
	free(message);
}

void webinterfaceSendData(void)
{
	// First check if our web handle is valid
	if (g_httpdHandle == NULL || !g_websocketConnected) {
		return;
	}

	// TODO: Fix httpd error 104/128
	// Send the message
	// if (httpd_queue_work(httpdHandle_, webinterfaceBroadcastWorker, (char*)data) != ESP_OK) {
	// 	// Queuing failed, free the memory
	// 	free((char*)data);
	// }
}

static void wifiEventHandler(void* p_arg, const esp_event_base_t p_eventBase, const int32_t eventID, void* p_eventData)
{
	// Connecting to WiFI AP
	if (p_eventBase == WIFI_EVENT && eventID == WIFI_EVENT_STA_START) {
		esp_rom_printf("Connecting to WiFi...\n");
		esp_wifi_connect();
	}

	// Disconnected from WiFi AP
	else if (p_eventBase == WIFI_EVENT && eventID == WIFI_EVENT_STA_DISCONNECTED) {
		esp_rom_printf("Disconnected, retrying...\n");
		esp_wifi_connect();
	}

	// Successfully connected to WiFi AP
	else if (p_eventBase == IP_EVENT && eventID == IP_EVENT_STA_GOT_IP) {
		const ip_event_got_ip_t* event = (ip_event_got_ip_t*)p_eventData;
		esp_rom_printf("Got IP assigned: %d.%d.%d.%d \n", IP2STR(&event->ip_info.ip));
	}
}

bool startWebInterface(WIFI_TYPE wifiType)
{
	bool success = true;

	/*
	 *	Set default Values
	 */
	// Set JOIN_AP values
	g_joinApSSID = (char*)malloc(sizeof("UNKNOWN"));
	strcpy(g_joinApSSID, "UNKNOWN");
	g_joinApPassword = (char*)malloc(sizeof("UNKNOWN"));
	strcpy(g_joinApPassword, "UNKNOWN");

	// Set HOST_AP values
	g_hostApSSID = (char*)malloc(sizeof("MX5-HybridDash Sensor Board"));
	strcpy(g_hostApSSID, "MX5-HybridDash Sensor Board");
	g_hostApPassword = (char*)malloc(sizeof("MX5-HybridDashV2"));
	strcpy(g_hostApPassword, "MX5-HybridDashV2");

	/*
	 *	Open the configuration file
	 */
	// Get the wifi config file
	cJSON* wifiJson = getWifiConfiguration();
	if (wifiJson != NULL) {
		// Read the WIFI_MODE data
		const cJSON* wifiMode = cJSON_GetObjectItem(wifiJson, "wifiMode");

		// Check if the value is valid and if we should alter the mode
		if (cJSON_IsString(wifiMode) && wifiMode->valuestring != NULL && wifiType == GET_FROM_CONFIG) {
			// HOST_AP
			if (strcmp(wifiMode->valuestring, "HOST_AP") == 0) {
				wifiType = HOST_AP;
			}
			// JOIN_AP
			else if (strcmp(wifiMode->valuestring, "JOIN_AP") == 0) {
				wifiType = JOIN_AP;
			}
		}

		// Read the JOIN_AP data
		const cJSON* joinApSSID = cJSON_GetObjectItem(wifiJson, "joinApSSID");
		const cJSON* joinApPassword = cJSON_GetObjectItem(wifiJson, "joinApPassword");

		// Check if they are valid
		if (cJSON_IsString(joinApSSID) && joinApSSID->valuestring != NULL) {
			free(g_joinApSSID);
			g_joinApSSID = malloc(strlen(joinApSSID->valuestring));
			strcpy(g_joinApSSID, joinApSSID->valuestring);
		}
		if (cJSON_IsString(joinApPassword) && joinApPassword->valuestring != NULL) {
			free(g_joinApPassword);
			g_joinApPassword = malloc(strlen(joinApPassword->valuestring));
			strcpy(g_joinApPassword, joinApPassword->valuestring);
		}

		// Read the HOST_AP data
		const cJSON* hostApSSID = cJSON_GetObjectItem(wifiJson, "hostApSSID");
		const cJSON* hostApPassword = cJSON_GetObjectItem(wifiJson, "hostApPassword");

		// Check if they are valid
		if (cJSON_IsString(hostApSSID) && hostApSSID->valuestring != NULL) {
			free(g_hostApSSID);
			g_hostApSSID = malloc(strlen(hostApSSID->valuestring));
			strcpy(g_hostApSSID, hostApSSID->valuestring);
		}
		if (cJSON_IsString(hostApPassword) && hostApPassword->valuestring != NULL) {
			free(g_hostApPassword);
			g_hostApPassword = malloc(strlen(hostApPassword->valuestring));
			strcpy(g_hostApPassword, hostApPassword->valuestring);
		}
	}

	/*
	 *	Start the Wi-Fi
	 */
	// Initialize NVS
	success &= nvs_flash_init() == ESP_OK;

	// Initialize TCP/IP
	success &= esp_netif_init() == ESP_OK;
	success &= esp_event_loop_create_default() == ESP_OK;

	// Initialize the HOST_AP mode
	if (wifiType == HOST_AP) {
		// Logging
		loggerInfo("Setting up AP...");

		// Create an instance
		esp_netif_create_default_wifi_ap();

		// Load the default config
		const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
		success &= esp_wifi_init(&initConfig);

		// Create the WiFi config
		wifi_config_t wifiConfig;
		wifiConfig = (wifi_config_t){
			.ap = {
				//.ssid = hostApSSID_,
				.ssid_len = strlen(g_hostApSSID),
				.channel = 1,
				//.password = hostApPassword_,
				.max_connection = 4,
				.authmode = WIFI_AUTH_WPA2_PSK,
			}
		};
		strlcpy((char*)wifiConfig.ap.ssid, g_hostApSSID, sizeof(wifiConfig.ap.ssid));
		strlcpy((char*)wifiConfig.ap.password, g_hostApPassword, sizeof(wifiConfig.ap.password));

		// Start up the Wi-Fi AP
		success &= esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK;
		success &= esp_wifi_set_config(WIFI_IF_AP, &wifiConfig) == ESP_OK;
		success &= esp_wifi_start() == ESP_OK;
	}
	// Initialize the JOIN_AP mode
	else if (wifiType == JOIN_AP) {
		loggerInfo("Joining AP...");

		// Create an instance
		const esp_netif_t* netif = esp_netif_create_default_wifi_sta();

		// Set the name of the device
		// esp_netif_set_hostname((esp_netif_t*)netif, "HybridDash");

		// Load the initial config
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		success &= esp_wifi_init(&cfg) == ESP_OK;

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
		strlcpy((char*)wifiConfig.sta.ssid, g_joinApSSID, sizeof(wifiConfig.sta.ssid));
		strlcpy((char*)wifiConfig.sta.password, g_joinApPassword, sizeof(wifiConfig.sta.password));

		// Register the event handler
		success &=
			esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL, NULL) == ESP_OK;
		success &=
			esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, NULL, NULL) == ESP_OK;

		// Start up the Wi-Fi connection
		success &= esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK;
		success &= esp_wifi_set_config(WIFI_IF_STA, &wifiConfig) == ESP_OK;
		success &= esp_wifi_start() == ESP_OK;
	}
	// Unknown WiFi mode
	else {
		loggerWarn("Read invalid WiFi mode from config. WiFi has been disabled.");
	}

	/*
	 *	Start the Webserver
	 */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.uri_match_fn = httpd_uri_match_wildcard;

	// Start the server
	success &= httpd_start(&g_httpdHandle, &config) == ESP_OK;

	// Register the URIs
	success &= httpd_register_uri_handler(g_httpdHandle, &g_webinterfaceWebsocketHandlerURI);
	success &= httpd_register_uri_handler(g_httpdHandle, &g_webinterfaceApiGETHandlerURI);
	success &= httpd_register_uri_handler(g_httpdHandle, &g_webinterfaceApiPOSTHandlerURI);
	success &= httpd_register_uri_handler(g_httpdHandle, &g_webinterfaceHandlerURI);
	return success;
}
