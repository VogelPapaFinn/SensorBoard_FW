// Project includes
#include "WebInterface.h"

// espidf includes
#include <esp_http_server.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>
#include <sys/dirent.h>

#include "../../esp-idf/components/json/cJSON/cJSON.h"
#include "logger.h"

/*
 *	Defines
 */
#define FILE_UPLOAD_BUFFER_SIZE_B 1024

/*
 *	Prototypes
 */
void websocketSendAsyncData();

static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventID, void* eventData);

/*
 *	File Handler
 */
static const char* getMimeType(const char* filepath)
{
	const char* lastDot = strrchr(filepath, '.');
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
static esp_err_t fileHandler(httpd_req_t* req)
{
	// Array which holds the file path of the file we return
	char filepath[600];

	// Check if it is the index.html file
	if (strcmp(req->uri, "/") == 0) {
		strcpy(filepath, "/spiffs/webinterface/index.html");
		websocketSendAsyncData();
	}
	else if (strcmp(req->uri, "/ws") == 0) {
		return ESP_OK;
	}
	else {
		// Otherwise build the absolute path to the requested file
		snprintf(filepath, sizeof(filepath), "/spiffs/webinterface%s", req->uri);
	}

	// Then open the file as read only
	FILE* reqFile = fopen(filepath, "r");
	if (reqFile == NULL) {
		// Error handling
		esp_rom_printf("Couldn't open file: %s\n", filepath);
		return ESP_FAIL;
	}

	// Set the MIME Type
	httpd_resp_set_type(req, getMimeType(filepath));

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
			if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
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

	// Send one last emptz chunk to signalize the end of the transmission
	httpd_resp_send_chunk(req, NULL, 0);

	return ESP_OK;
}


/*
 *	Other Handlers
 */

/*
 *	URIs
 */
static const httpd_uri_t fileHandlerURI = {
	.uri = "/*", .method = HTTP_GET, .handler = fileHandler, .user_ctx = NULL, .is_websocket = true};

/*
 *	Private Variables
 */
httpd_handle_t httpdHandle_ = NULL;

char* joinApSSID_ = NULL;
char* joinApPassword_ = NULL;
char* hostApSSID_ = NULL;
char* hostApPassword_ = NULL;


/*
 *	Function implementations
 */
// Diese Funktion läuft im Webserver-Thread (sicher)
struct WsMessage
{
	char* payload;
};

void ws_broadcast_worker(void* arg)
{
	httpd_handle_t hd =
		(httpd_handle_t)arg; // Wir brauchen das Handle, hier nutzen wir globalen Context oder übergeben es anders
	// HINWEIS: Wenn 'server' global ist, nutze einfach die globale Variable 'server' statt 'hd' cast.
	// Unten im Beispiel gehe ich davon aus, dass 'httpdHandle_' global verfügbar ist.

	struct WsMessage* msg = (struct WsMessage*)arg;

	// 1. Clients abrufen
	size_t fds = 4;
	int client_fds[4];
	// Fehlerbehandlung: Sicherstellen, dass httpdHandle_ existiert
	if (httpd_get_client_list(httpdHandle_, &fds, client_fds) == ESP_OK) {

		// 2. Paket schnüren
		httpd_ws_frame_t packet;
		packet.payload = (uint8_t*)msg->payload;
		packet.len = strlen(msg->payload);
		packet.type = HTTPD_WS_TYPE_TEXT;

		// 3. An alle Clients senden
		for (int i = 0; i < fds; i++) {
			// Hier nutzen wir die SYNC Version, da wir schon im Worker-Thread sind!
			// Das ist sicherer und wir können den Speicher danach sofort freigeben.
			if (client_fds[i] == 0)
				continue;
			esp_rom_printf("Sending message\n");
			httpd_ws_send_frame_async(httpdHandle_, client_fds[i], &packet);
		}
	}

	// 4. Speicher aufräumen (WICHTIG!)
	free(msg->payload); // Den JSON String löschen
	free(msg); // Die Message Struktur löschen
}

// Diese Funktion rufst du aus deinem Loop auf
void websocketSendAsyncData()
{
	if (httpdHandle_ == NULL)
		return;

	// 1. JSON erstellen
	cJSON* json = cJSON_CreateObject();
	cJSON_AddNumberToObject(json, "test", 123);
	cJSON_AddNumberToObject(json, "test2", 456);

	// 2. String drucken (reserviert RAM!)
	char* jsonStr = cJSON_PrintUnformatted(json);

	// cJSON Objekt können wir jetzt schon löschen, der String existiert ja separat
	cJSON_Delete(json);

	if (jsonStr == NULL)
		return; // Kein Speicher mehr?

	// 3. Message für den Worker vorbereiten
	struct WsMessage* msg = (struct WsMessage*)malloc(sizeof(struct WsMessage));
	if (msg) {
		msg->payload = jsonStr; // Pointer übergeben (Worker gibt ihn frei)

		// 4. In die Queue schieben
		// Wir übergeben 'msg' als Argument an den Worker
		if (httpd_queue_work(httpdHandle_, ws_broadcast_worker, msg) != ESP_OK) {
			// Falls Queue voll ist: Selbst aufräumen, sonst Leak!
			free(jsonStr);
			free(msg);
		}
	}
	else {
		free(jsonStr);
	}
}

static void wifiEventHandler(void* arg, esp_event_base_t eventBase, int32_t eventID, void* eventData)
{
	// Connecting to WiFI AP
	if (eventBase == WIFI_EVENT && eventID == WIFI_EVENT_STA_START) {
		esp_rom_printf("Connecting to WiFi...\n");
		esp_wifi_connect();
	}

	// Disconnected from WiFi AP
	else if (eventBase == WIFI_EVENT && eventID == WIFI_EVENT_STA_DISCONNECTED) {
		esp_rom_printf("Disconnected, retrying...\n");
		esp_wifi_connect();
	}

	// Successfully connected to WiFi AP
	else if (eventBase == IP_EVENT && eventID == IP_EVENT_STA_GOT_IP) {
		const ip_event_got_ip_t* event = (ip_event_got_ip_t*)eventData;
		esp_rom_printf("Got IP assigned: %d.%d.%d.%d \n", IP2STR(&event->ip_info.ip));
	}
}

bool startWebInterface(WIFI_TYPE wifiType)
{
	bool success = true;

	/*
	 *	Mount the data partition where the webinterface is
	 */
	// Create the partition config for mounting
	const esp_vfs_spiffs_conf_t partitionConfig = {
		.base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = false};

	// Mount the partition
	success &= esp_vfs_spiffs_register(&partitionConfig) == ESP_OK;

	/*
	 *	Set default Values
	 */
	// Set JOIN_AP values
	joinApSSID_ = (char*)malloc(sizeof("UNKNOWN"));
	strcpy(joinApSSID_, "UNKNOWN");
	joinApPassword_ = (char*)malloc(sizeof("UNKNOWN"));
	strcpy(joinApPassword_, "UNKNOWN");

	// Set HOST_AP values
	hostApSSID_ = (char*)malloc(sizeof("MX5-HybridDash Control Board"));
	strcpy(hostApSSID_, "MX5-HybridDash Control Board");
	hostApPassword_ = (char*)malloc(sizeof("MX5-HybridDashV2"));
	strcpy(hostApPassword_, "MX5-HybridDashV2");

	/*
	 *	Open the configuration file
	 */
	const FILE* cfgFile = fopen("/spiffs/config.json", "r");

	// If it worked pull all needed values
	if (cfgFile != NULL) {
		// Copy the file content into a file
		char fileBuffer[1024];
		fread(fileBuffer, 1, 1024, (FILE*)cfgFile);

		// Close the file
		fclose((FILE*)cfgFile);

		// Parse the cJSON data
		const cJSON* json = cJSON_Parse(fileBuffer);
		if (json != NULL) {
			// Read the WIFI_MODE data
			const cJSON* wifiMode = cJSON_GetObjectItem(json, "wifiMode");

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
			const cJSON* joinApSSID = cJSON_GetObjectItem(json, "joinApSSID");
			const cJSON* joinApPassword = cJSON_GetObjectItem(json, "joinApPassword");

			// Check if they are valid
			if (cJSON_IsString(joinApSSID) && joinApSSID->valuestring != NULL) {
				strcpy(joinApSSID_, joinApSSID->valuestring);
			}
			if (cJSON_IsString(joinApPassword) && joinApPassword->valuestring != NULL) {
				strcpy(joinApPassword_, joinApPassword->valuestring);
			}

			// Read the HOST_AP data
			const cJSON* hostApSSID = cJSON_GetObjectItem(json, "hostApSSID");
			const cJSON* hostApPassword = cJSON_GetObjectItem(json, "hostApPassword");

			// Check if they are valid
			if (cJSON_IsString(hostApSSID) && hostApSSID->valuestring != NULL) {
				strcpy(hostApSSID_, hostApSSID->valuestring);
			}
			if (cJSON_IsString(hostApPassword) && hostApPassword->valuestring != NULL) {
				strcpy(hostApPassword_, hostApPassword->valuestring);
			}

			// Free memory
			cJSON_Delete((cJSON*)json);
		}
		else {
			loggerWarn("Couldn't parse config file");
		}
	}
	else {
		loggerWarn("Couldn't open config file");
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
		wifiConfig = (wifi_config_t){.ap = {
										 //.ssid = hostApSSID_,
										 .ssid_len = strlen(hostApSSID_),
										 .channel = 1,
										 //.password = hostApPassword_,
										 .max_connection = 4,
										 .authmode = WIFI_AUTH_WPA2_PSK,
									 }};
		strlcpy((char*)wifiConfig.ap.ssid, hostApSSID_, sizeof(wifiConfig.ap.ssid));
		strlcpy((char*)wifiConfig.ap.password, hostApPassword_, sizeof(wifiConfig.ap.password));

		// Start up the Wi-Fi AP
		success &= esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK;
		success &= esp_wifi_set_config(WIFI_IF_AP, &wifiConfig) == ESP_OK;
		success &= esp_wifi_start() == ESP_OK;
	}
	// Initialize the JOIN_AP mode
	else if (wifiType == JOIN_AP) {
		loggerInfo("Joining AP...");

		// Create an instance
		esp_netif_create_default_wifi_sta();

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
		strlcpy((char*)wifiConfig.sta.ssid, joinApSSID_, sizeof(wifiConfig.sta.ssid));
		strlcpy((char*)wifiConfig.sta.password, joinApPassword_, sizeof(wifiConfig.sta.password));

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
	success &= httpd_start(&httpdHandle_, &config) == ESP_OK;

	// Register the URIs
	success &= httpd_register_uri_handler(httpdHandle_, &fileHandlerURI);

	return success;
}
