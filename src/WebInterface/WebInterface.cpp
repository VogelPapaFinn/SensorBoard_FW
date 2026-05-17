#include "WebInterface/WebInterface.hpp"

// Project includes
#include "Driver/EcuSensors.hpp"

// C++ includes
#include <sstream>

/*
 *	Prototypes
 */
static esp_err_t fileHandler(httpd_req_t* p_reqst);
static esp_err_t staticWebsocketHandler(httpd_req_t* p_reqst);

/*
 *	constexpr
 */
constexpr auto TAG = "WebInterface";

constexpr auto JSON_WIFI_MODE = "OverwriteWifiMode";
constexpr auto JSON_WIFI_HOST = "WifiHost";
constexpr auto JSON_WIFI_JOIN = "WifiJoin";

constexpr uint16_t FILE_CHUNK_SIZE_B = 1024;

constexpr uint16_t WEBSOCKET_RECV_BUFFER_B = 256;

constexpr httpd_uri_t FILE_URI = {
	.uri = "/*", .method = HTTP_GET, .handler = fileHandler, .user_ctx = nullptr
};

/*
 *	Private Static ISR
 */
static std::string getMimeType(const std::string& filepath)
{
	// Get file ending
	const size_t pos = filepath.find_last_of(".");
	if (pos == std::string::npos) {
		return "text/plain";
	}

	const std::string lastDot = filepath.substr(pos + 1);
	if (lastDot == "html") {
		return "text/html";
	}
	if (lastDot == "css") {
		return "text/css";
	}
	if (lastDot == "js") {
		return "application/javascript";
	}
	if (lastDot == "png") {
		return "image/png";
	}
	if (lastDot == "jpg") {
		return "image/jpeg";
	}
	if (lastDot == "ico") {
		return "image/x-icon";
	}

	return "text/plain";
}

static esp_err_t fileHandler(httpd_req_t* p_reqst)
{
	std::string filepath;

	if (strcmp(p_reqst->uri, "") == 0) {
		return ESP_ERR_INVALID_ARG;
	}

	// Return the specified file
	if (strcmp(p_reqst->uri, "/") == 0) {
		filepath = "webinterface/index.html";
	}
	else {
		filepath = "webinterface";
		filepath += p_reqst->uri;
	}

	// Then open the file as read only
	FILE* reqFile = Filesystem::get()->openFile(filepath, "r", Filesystem::DATA_PARTITION);
	if (reqFile == nullptr) {
		esp_rom_printf("[%s] Couldn't open file: %s\n", TAG, filepath.c_str());
		return ESP_FAIL;
	}

	httpd_resp_set_type(p_reqst, getMimeType(filepath).c_str());

	/*
	 * Send the file in chunks
	 */
	char chunk[FILE_CHUNK_SIZE_B] = {0x00};
	ssize_t chunkSize;
	do {
		chunkSize = fread(chunk, 1, FILE_CHUNK_SIZE_B, reqFile);

		if (chunkSize <= 0) {
			continue;
		}

		if (httpd_resp_send_chunk(p_reqst, chunk, chunkSize) != ESP_OK) {
			fclose(reqFile);
			return ESP_FAIL;
		}
	}
	while (chunkSize != 0);

	fclose(reqFile);

	// Send one last empty chunk to signalize the end of the transmission
	httpd_resp_send_chunk(p_reqst, nullptr, 0);

	return ESP_OK;
}

struct WsContext
{
	WebInterface* web = nullptr;
	int clientFd = 0;
};
static void staticWebsocketCrashedHandler(void *ctx)
{
	if (ctx == nullptr) {
		return;
	}

	const WsContext* wsContext = static_cast<WsContext*>(ctx);
	wsContext->web->websocketCrashed(wsContext->clientFd);

	delete wsContext;
}

static esp_err_t staticWebsocketHandler(httpd_req_t* p_reqst)
{
	if (p_reqst->user_ctx == nullptr) {
		return ESP_FAIL;
	}

	const auto web = static_cast<WebInterface*>(p_reqst->user_ctx);

	if (p_reqst->method == HTTP_GET) {
		ESP_LOGI(TAG, "Websocket handshake successful");

		WsContext* ctx = new WsContext();
		ctx->web = web;
		ctx->clientFd = httpd_req_to_sockfd(p_reqst);;

		p_reqst->sess_ctx = (void*)ctx;
		p_reqst->free_ctx = staticWebsocketCrashedHandler;
		return ESP_OK;
	}

	return web->websocketHandler(p_reqst);
}

static void updateSensorsTask(void* param)
{
	if (param == nullptr) {
		vTaskDelete(nullptr);
	}

	WebInterface* web = static_cast<WebInterface*>(param);
	auto mutex = web->getSensorsMutex();

	while (true) {
		xSemaphoreTakeRecursive(mutex, portMAX_DELAY);

		auto trackedSensorsMap = web->getTrackedSensors();
		for (auto& fdPair : trackedSensorsMap) {
			auto& trackedSensorsVector = fdPair.second;

			// JSON Header
			std::stringstream output;
			output << "{";
			output << "\"type\":\"update-sensors\",";
			output << "\"sensors\":" << "[";

			// Parse all sensors to JSON
			for (auto& sensorId : trackedSensorsVector) {
				output << ECU_SENSORS[sensorId].toJson();

				// Add ',' if it's not the last sensor we add
				if (sensorId != trackedSensorsVector.at(trackedSensorsVector.size() - 1)) {
					output << ",";
				}
			}

			// JSON Ending
			output << "]";
			output << "}";

			web->send(fdPair.first, output.str());
		}

		xSemaphoreGiveRecursive(mutex);

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

/*
 *	Public Function Implementations
 */
WebInterface::WebInterface(const bool initiateWifiDriver)
{
	sensorsMutex_ = xSemaphoreCreateMutex();

	core_ = Core::get();
	config_ = core_->getConfig();
	if (config_->isNull()) {
		ESP_LOGE(TAG, "Failed to load config");
		return;
	}

	if (initiateWifiDriver) {
		if ((*config_)[JSON_WIFI_MODE]) {
			wifiMode_ = (*config_)[JSON_WIFI_MODE].as<std::string>();
		}

		if (wifiMode_ == "") {
			ESP_LOGI(TAG, "Wifi mode was empty. Aborting");
			return;
		}

		if (wifiMode_ == JSON_WIFI_HOST) {
			wifiHost_.start();
		}
		if (wifiMode_ == JSON_WIFI_JOIN) {
			wifiJoin_.callOnConnect([this] { onConnectedToWifi(); });
			wifiJoin_.connect();
			return;
		}
	}

	onConnectedToWifi();
}

void WebInterface::send(const int clientFD, const std::string& data) const
{
	if (!initialized_) { return; }

	if (httpd_ws_get_fd_info(httpdHandle_, clientFD) != HTTPD_WS_CLIENT_WEBSOCKET) {
		ESP_LOGW(TAG, "Cant send to Frontend. Server Handle or ClientFD is invalid");
		return;
	}

	httpd_ws_frame_t frame = {};
	frame.payload = (uint8_t*)data.c_str();
	frame.len = data.length();
	frame.type = HTTPD_WS_TYPE_TEXT;
	frame.final = true;

	httpd_ws_send_frame_async(httpdHandle_, clientFD, &frame);
}

std::unordered_map<int, std::vector<uint16_t>>& WebInterface::getTrackedSensors()
{
	return trackedSensors_;
}

SemaphoreHandle_t& WebInterface::getSensorsMutex()
{
	return sensorsMutex_;
}

/*
 *	Private ISRs
 */
esp_err_t WebInterface::websocketHandler(httpd_req_t* p_reqst)
{
	const int clientFD = httpd_req_to_sockfd(p_reqst);

	// Prepare rx frame slot
	httpd_ws_frame_t frame = {};
	frame.type = HTTPD_WS_TYPE_TEXT;
	char data[WEBSOCKET_RECV_BUFFER_B] = {};
	frame.payload = reinterpret_cast<uint8_t*>(data);

	// Get frame data
	if (httpd_ws_recv_frame(p_reqst, &frame, WEBSOCKET_RECV_BUFFER_B) != ESP_OK) {
		ESP_LOGW(TAG, "Failed to fetch frame");
		return ESP_FAIL;
	}
	std::string dataStr = data;

	// DEBUG LOGGING
	ESP_LOGI(TAG, "Received string from Frontend: %s", data);

	/*
	 * Act depending on the frame data
	 */
	if (dataStr == "fetch-sensors") {
		sendAllSensors(clientFD);
		return ESP_OK;
	}

	if (dataStr.contains("add-sensor")) {
		const std::string sensorId = dataStr.substr(dataStr.find(':') + 1);

		xSemaphoreTakeRecursive(sensorsMutex_, portMAX_DELAY);
		trackedSensors_[clientFD].push_back(std::stoi(sensorId));
		xSemaphoreGiveRecursive(sensorsMutex_);

		if (updateSensorsDataTask_ != nullptr) {
			return ESP_OK;
		}

		if (xTaskCreate(updateSensorsTask, "WebInterfaceUpdateSensorsTask", 4096, this, 2,
		                &updateSensorsDataTask_) != pdPASS) {
			updateSensorsDataTask_ = nullptr;
			ESP_LOGW(TAG, "Failed to start task which updates the sensor values");
			return ESP_OK;
		}

		return ESP_OK;
	}

	if (dataStr.contains("remove-sensor")) {
		const std::string sensorIdStr = dataStr.substr(dataStr.find(':') + 1);
		const int sensorId = std::stoi(sensorIdStr);

		auto& sensorVector = trackedSensors_[clientFD];
		for (int i = 0; i < sensorVector.size(); i++) {
			if (sensorVector.at(i) == sensorId) {
				xSemaphoreTakeRecursive(sensorsMutex_, portMAX_DELAY);
				sensorVector.erase(sensorVector.begin() + i);
				xSemaphoreGiveRecursive(sensorsMutex_);
				break;
			}
		}

		if (updateSensorsDataTask_ == nullptr) {
			return ESP_OK;
		}

		vTaskDelete(updateSensorsDataTask_);
		updateSensorsDataTask_ = nullptr;

		return ESP_OK;
	}

	return ESP_OK;
}

void WebInterface::websocketCrashed(const int fd)
{
	if (!trackedSensors_.contains(fd)) {
		return;
	}

	trackedSensors_.erase(fd);

	if (!trackedSensors_.empty()) {
		return;
	}


	if (updateSensorsDataTask_ == nullptr) {
		return;
	}

	vTaskDelete(updateSensorsDataTask_);
	updateSensorsDataTask_ = nullptr;
}

/*
 *	Private Functions Implementations
 */
void WebInterface::onConnectedToWifi()
{
	httpdConfig_ = HTTPD_DEFAULT_CONFIG();
	httpdConfig_.uri_match_fn = httpd_uri_match_wildcard;
	httpdConfig_.stack_size = 8192;
	httpdConfig_.lru_purge_enable = true;

	if (httpd_start(&httpdHandle_, &httpdConfig_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start webserver");
		return;
	}

	const httpd_uri_t websocketUri = {
		.uri = "/ws",
		.method = HTTP_GET,
		.handler = staticWebsocketHandler,
		.user_ctx = this,
		.is_websocket = true
	};
	if (httpd_register_uri_handler(httpdHandle_, &websocketUri) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to register Websocket URI");
		return;
	}

	if (httpd_register_uri_handler(httpdHandle_, &FILE_URI) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to register File URI");
		return;
	}

	ESP_LOGI(TAG, "Initialized");
	initialized_ = true;
}

void WebInterface::sendAllSensors(const int clientFD) const
{
	std::stringstream output;
	output << "{";
	output << "\"type\":\"fetch-sensors\",";
	output << "\"sensors\":" << "[";

	// Parse all sensors to JSON
	for (auto it = ECU_SENSORS.begin(); it != ECU_SENSORS.end(); ++it) {
		output << it->second.toJson();

		if (std::next(it) != ECU_SENSORS.end()) {
			output << ",";
		}
	}

	output << "]";
	output << "}";

	send(clientFD, output.str());
}
