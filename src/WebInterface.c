// Project includes
#include "WebInterface.h"

// espidf includes
#include <esp_http_server.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>
#include <sys/dirent.h>

#include "esp_private/mmu_psram_flash.h"
#include "esp_spiffs.h"

/*
 *	Defines
 */
#define WIFI_SSID "MX5-HybridDash Control Board"
#define WIFI_PASSWORD "MX5-HybridDashV2"

#define FILE_UPLOAD_BUFFER_SIZE_B 1024

/*
 *	HTML, CSS & JS files
 */
// index.html
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// style.css
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

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
		strcpy(filepath, "/spiffs/index.html");
	}
	else {
		// Otherwise build the absolute path to the requested file
		snprintf(filepath, sizeof(filepath), "/spiffs%s", req->uri);
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
static esp_err_t button_post_handler(httpd_req_t* req)
{
	// 1. Die C-Funktion aufrufen
	test();

	// 2. Dem Browser antworten, dass es geklappt hat
	// Wir senden einfach "OK" zurück
	httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

	return ESP_OK;
}

static esp_err_t upload_post_handler(httpd_req_t* req)
{
	char buf[FILE_UPLOAD_BUFFER_SIZE_B];
	int ret, remaining = req->content_len;

	// esp_rom_printf("Empfange Upload. Gesamtgröße: %d Bytes", remaining);

	// Schleife, solange noch Daten ausstehen
	while (remaining > 0) {
		/*
		 * Daten empfangen
		 * Wir versuchen, entweder den Rest der Datei ODER maximal die Buffergröße zu lesen.
		 */
		if ((ret = httpd_req_recv(req, buf, MIN(remaining, FILE_UPLOAD_BUFFER_SIZE_B))) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				// Timeout: Wir können es nochmal versuchen (continue)
				continue;
			}
			// Schwerwiegender Fehler (Verbindung abgebrochen etc.)
			return ESP_FAIL;
		}

		/* * -----------------------------------------------------------
		 * HIER PASSIERT IHRE VERARBEITUNG
		 * -----------------------------------------------------------
		 * 'buf' enthält jetzt 'ret' Bytes an Daten.
		 * * Beispiele:
		 * - Schreiben in eine Datei (fwrite auf SPIFFS/SD-Card)
		 * - Parsen von JSON/Config
		 * - OTA Update (esp_ota_write)
		 */

		// Beispiel: Einfach nur Loggen der ersten paar Bytes
		// process_data(buf, ret);
		// esp_rom_printf("Verarbeite Chunk von %d Bytes...", ret);

		// Zähler aktualisieren
		remaining -= ret;
	}

	// Antwort an den Browser senden, dass alles geklappt hat
	httpd_resp_sendstr(req, "Datei erfolgreich hochgeladen");
	return ESP_OK;
}

/*
 *	URIs
 */
static const httpd_uri_t fileHandlerURI = {.uri = "/*", .method = HTTP_GET, .handler = fileHandler, .user_ctx = NULL};

static const httpd_uri_t button_uri = {.uri = "/trigger", // Die URL, die der Button aufruft
									   .method = HTTP_POST, // POST ist besser für Aktionen als GET
									   .handler = button_post_handler,
									   .user_ctx = NULL};

static const httpd_uri_t upload_uri = {
	.uri = "/upload", .method = HTTP_POST, .handler = upload_post_handler, .user_ctx = NULL};


/*
 *	Private Variables
 */
const wifi_config_t wifiConfig_ = {.ap = {
									   .ssid = WIFI_SSID,
									   .ssid_len = strlen(WIFI_SSID),
									   .channel = 1,
									   .password = WIFI_PASSWORD,
									   .max_connection = 4,
									   .authmode = WIFI_AUTH_WPA2_PSK,
								   }};
httpd_handle_t httpdHandle_ = NULL;

/*
 *	Function implementations
 */
bool startWebInterface(void)
{
	bool success = false;

	/*
	 *	Start the Wi-Fi
	 */
	// Initialize NVS
	success &= nvs_flash_init() == ESP_OK;

	// Initialize TCP/IP
	success &= esp_netif_init() == ESP_OK;
	success &= esp_event_loop_create_default() == ESP_OK;

	// Initialize the AP mode
	esp_netif_create_default_wifi_ap();

	// Load the default config
	const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
	success &= esp_wifi_init(&initConfig);

	// Start up the Wi-Fi
	success &= esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK;
	success &= esp_wifi_set_config(WIFI_IF_AP, &wifiConfig_) == ESP_OK;
	success &= esp_wifi_start() == ESP_OK;

	/*
	 *	Mount the data partition where the webinterface is
	 */
	// Create the partition config for mounting
	const esp_vfs_spiffs_conf_t partitionConfig = {
		.base_path = "/spiffs", .partition_label = NULL, .max_files = 5, .format_if_mount_failed = false};

	// Mount the partition
	success &= esp_vfs_spiffs_register(&partitionConfig) == ESP_OK;

	/*
	 *	Start the Webserver
	 */
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.uri_match_fn = httpd_uri_match_wildcard;

	// Start the server
	success &= httpd_start(&httpdHandle_, &config) == ESP_OK;

	// Register the URIs
	success &= httpd_register_uri_handler(httpdHandle_, &fileHandlerURI);
	success &= httpd_register_uri_handler(httpdHandle_, &button_uri);
	success &= httpd_register_uri_handler(httpdHandle_, &upload_uri);

	return success;
}
