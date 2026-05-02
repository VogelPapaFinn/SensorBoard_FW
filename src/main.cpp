// Project includes
#include "Can.hpp"
#include "Driver/Display.hpp"
#include "Driver/Filesystem.hpp"
#include "Config.hpp"

// espidf includes
#include <freertos/FreeRTOS.h>

QueueHandle_t queue = xQueueCreate(10, sizeof(Can::Frame));
static void canTask(void* p_param)
{
	// Wait for new queue events
	Can::Frame rxFrame;
	while (true) {
		if (xQueueReceive(queue, &rxFrame, portMAX_DELAY) != pdPASS) {
			continue;
		}

		esp_rom_printf("RX CB Triggered!\n");
	}
}

#include "ArduinoJson.hpp"
extern "C" void app_main(void)
{
	Filesystem* fs = Filesystem::get();
	fs->test();

	Config cfg("displays_config.json");
	ArduinoJson::JsonDocument* doc = cfg.getJson();

	std::string output = "";
	ArduinoJson::serializeJsonPretty(*doc, output);
	ESP_LOGI("main", "%s", output.c_str());

	cfg.save();

	output = "";
	ArduinoJson::serializeJsonPretty(*doc, output);
	ESP_LOGI("main", "%s", output.c_str());

	std::string c = (*doc)["displayConfigurations"][0]["hwUuid"];
	ESP_LOGI("main", "displayConfigurations: %s", c.c_str());
	(*doc)["displayConfigurations"][0]["hwUuid"] = "123";

	output = "";
	ArduinoJson::serializeJsonPretty(*doc, output);
	ESP_LOGI("main", "%s", output.c_str());

	cfg.save();

	output = "";
	ArduinoJson::serializeJsonPretty(*doc, output);
	ESP_LOGI("main", "%s", output.c_str());

	//
	//Config cfg2("displays_config.json");
	//
	//ArduinoJson::JsonDocument* doc2 = cfg2.getJson();
	//
	//const char* c2 = (*doc2)["displayConfigurations"]["hwUuid"];
	//ESP_LOGI("main", "displayConfigurations: %s", c2);
	//(*doc2)["displayConfigurations"]["hwUuid"] = "123";
	//cfg2.save();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	Display d1(GPIO_NUM_14);
	d1.supplyPower();

	TaskHandle_t g_taskHandle;
	if (xTaskCreate(canTask, "CanUpdaterTask", 2048 * 4, NULL, 2, &g_taskHandle) != pdPASS) {
		esp_rom_printf("Failed\n");
	}

	Can canNode(GPIO_NUM_41, GPIO_NUM_40);
	canNode.initialize();
	canNode.enable();

	canNode.registerRxCbQueue(&queue);

	Can::Frame frame;
	frame.id = 0;
	frame.group = 0;
	frame.function = 0;

	while (true) {
		canNode.queueFrame(frame);
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}
