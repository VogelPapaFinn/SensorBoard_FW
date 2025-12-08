// Project includes
#include "WebInterface.h"

// espidf includes
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>

/*
 *	Defines
 */
#define WIFI_SSID "MX5-HybridDash Control Board"
#define WIFI_PASSWORD "MX5-HybridDashV2"

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
// index.html Handler
static esp_err_t root_get_handler(httpd_req_t* req)
{
	httpd_resp_set_type(req, "text/html");
	// Wir senden die Daten zwischen start und end
	// (end - start) ergibt die Länge der Datei
	httpd_resp_send(req, (const char*)index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

// style.css Handler
static esp_err_t style_get_handler(httpd_req_t* req)
{
	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char*)style_css_start, style_css_end - style_css_start);
	return ESP_OK;
}

/*
 *	Button Handlers
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

/*
 *	URIs
 */
// URI index.html
static const httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};

// URI style.css
static const httpd_uri_t style_uri = {
	.uri = "/style.css", .method = HTTP_GET, .handler = style_get_handler, .user_ctx = NULL};

static const httpd_uri_t button_uri = {.uri = "/trigger", // Die URL, die der Button aufruft
									   .method = HTTP_POST, // POST ist besser für Aktionen als GET
									   .handler = button_post_handler,
									   .user_ctx = NULL};


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
	 *	Start the Webserver
	 */
	const httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// Start the server
	success &= httpd_start(&httpdHandle_, &config) == ESP_OK;

	// Register the URIs
	success &= httpd_register_uri_handler(httpdHandle_, &root_uri);
	success &= httpd_register_uri_handler(httpdHandle_, &style_uri);
	success &= httpd_register_uri_handler(httpdHandle_, &button_uri);

	return success;
}
