// Project includes
#include "Can.hpp"
#include "Driver/Display.hpp"
#include "Events.hpp"
#include "Filesystem.hpp"
#include "Handler/RegistrationHandler.hpp"
#include "State/Operation.hpp"
#include "State/Registration.hpp"
#include "SystemContext.hpp"

// espidf includes
#include <freertos/FreeRTOS.h>

//! \brief Main application tag.
constexpr auto TAG = "main";

//! \brief GPIO pin number for CAN receiver.
constexpr gpio_num_t GPIO_CAN_RX = GPIO_NUM_41;

//! \brief GPIO pin number for CAN transmitter.
constexpr gpio_num_t GPIO_CAN_TX = GPIO_NUM_40;

//! \brief Default configuration file name.
constexpr auto DEFAULT_CONFIG_NAME = "default/config.json";

//! \brief Configuration file name.
constexpr auto CONFIG_NAME = "config.json";

constexpr adc_oneshot_unit_init_cfg_t ADC1_UNIT_CONFIG = {.unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE};

constexpr gpio_num_t GPIO_DISPLAY1 = GPIO_NUM_13;
constexpr gpio_num_t GPIO_DISPLAY2 = GPIO_NUM_14;
constexpr gpio_num_t GPIO_DISPLAY3 = GPIO_NUM_21;

/*
 *	Private Static Variables
 */
//! \brief Current application state.
static std::shared_ptr<State> g_currentState;

/*
 *	Helper functions
 */
//! \brief Registers necessary events for system initialization.
static void registerToEvents(SystemContext* p_sysCon)
{
	/*
	 *	Registration Completed
	 */
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, REGISTRATION_COMPLETED,
		[](void* p_systemContext, esp_event_base_t, int32_t, void*)
		{
			/*
			 *	Get the system context
			 */
			if (p_systemContext == nullptr) {
				return;
			}

			auto sysCon = static_cast<SystemContext*>(p_systemContext);

			/*
			 *	Enter the operation state
			 */
			g_currentState = std::make_shared<Operation>(sysCon);
			g_currentState->enter();
		},
		p_sysCon, nullptr);
}

//! \brief Creates and opens the configuration file.
//! \param sysCon The system context used for configuration.
static void createAndOpenConfigFile(SystemContext& sysCon)
{
	/*
	 *	Create default config, if config doesnt exist
	 */
	if (!sysCon.filesystem->doesFileExist(CONFIG_NAME, Filesystem::CONFIG_PARTITION)) {
		sysCon.filesystem->createFile(CONFIG_NAME, Filesystem::CONFIG_PARTITION);

		if (!sysCon.filesystem->doesFileExist(DEFAULT_CONFIG_NAME, Filesystem::CONFIG_PARTITION)) {
			Config defaultConfig(&sysCon);
			defaultConfig.open(DEFAULT_CONFIG_NAME);

			Config newConfig(&sysCon);
			newConfig.open(CONFIG_NAME);
			*newConfig.getJson() = *defaultConfig.getJson();

			newConfig.save();
		}
	}

	/*
	 *	Load the config file
	 */
	sysCon.config->open(CONFIG_NAME);
	const auto& jsonConfig = sysCon.config->getJson();
	if (jsonConfig != nullptr) {
		std::string str;
		serializeJsonPretty(*jsonConfig, str);
		ESP_LOGI(TAG, "%s", str.c_str());
	}
}

/*
 *	main function
 */
extern "C" void app_main(void)
{
	// NEEDED FOR DEBUGGING
	vTaskDelay(pdMS_TO_TICKS(100));

	/*
	 *	Print startup logging header
	 */
	ESP_LOGI(TAG, "--- --- --- --- --- --- ---");
	ESP_LOGI(TAG, "Startup");

	/*
	 *	Start the event loop
	 */
	if (esp_event_loop_create_default() != ESP_OK) {
		ESP_LOGE(TAG, "Error creating default event loop. Rebooting...");
		esp_restart();
	}

	/*
	 *	Create all necessary instances
	 */
	// System Context
	SystemContext sysCon;

	// Can
	Can can(GPIO_CAN_RX, GPIO_CAN_TX);
	can.initialize();
	can.enable();

	// Filesystem
	Filesystem fs;

	// Config
	Config config(&sysCon);

	// KLine
	KLine kline;

	// Displays
	Display display1(&sysCon, GPIO_DISPLAY2, CAN_MASTER_ID + 1, 0, true);
	Display display2(&sysCon, GPIO_DISPLAY1, CAN_MASTER_ID + 2, 1, false);
	Display display3(&sysCon, GPIO_DISPLAY3, CAN_MASTER_ID + 3, 2, false);

	// Registration Handler
	RegistrationHandler regHandler(&sysCon);

	/*
	 *	Setup the ADC1
	 */
	if (adc_oneshot_new_unit(&ADC1_UNIT_CONFIG, &sysCon.adc1) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize ADC1");
	}

	/*
	 *	Register the necessary events
	 */
	registerToEvents(&sysCon);

	/*
	 *	Build the SystemContext
	 */
	sysCon.can = &can;
	sysCon.filesystem = &fs;
	sysCon.config = &config;
	sysCon.displays = {&display1, &display2, &display3};
	sysCon.wifi = nullptr;
	sysCon.kline = &kline;

	/*
	 *	Ensure the config file exists and is loaded
	 */
	createAndOpenConfigFile(sysCon);

	/*
	 *	Create and enter the registration state
	 */
	g_currentState = std::make_shared<Registration>(&sysCon);
	g_currentState->enter();

	while (true) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
