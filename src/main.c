// Project includes
#include "Global.h"
#include "can.h"
#include "can_messages.h"
#include "logger.h"
#include "statemachine.h"

// espidf includes
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs_flash.h>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 *	Defines
 */
#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

// GPIOs
#define GPIO_OIL_PRESSURE GPIO_NUM_12
#define GPIO_FUEL_LEVEL GPIO_NUM_11
#define GPIO_WATER_TEMPERATURE GPIO_NUM_13
#define GPIO_SPEED GPIO_NUM_14
#define GPIO_RPM GPIO_NUM_21

// ADC CHANNELS
#define ADC_CHANNEL_OIL_PRESSURE ADC_CHANNEL_1
#define ADC_CHANNEL_FUEL_LEVEL ADC_CHANNEL_0
#define ADC_CHANNEL_WATER_TEMPERATURE ADC_CHANNEL_2
#define ADC_CHANNEL_INT_TEMPERATURE ADC_CHANNEL_6

// OIL PRESSURE THRESHOLDS
#define OIL_LOWER_VOLTAGE_THRESHOLD 65 // mV -> R2 ~= 5 Ohms
#define OIL_UPPER_VOLTAGE_THRESHOLD 255 // mV -> R2 ~= 20 Ohms

// FUEL LEVEL CALCULATION STUFF
#define FUEL_LEVEL_OFFSET 5.0f
#define FUEL_LEVEL_TO_PERCENTAGE 115.0f // Divide the calculated resistance by this value to get the level in percent


/*
 *	Prototypes
 */
void requestUUIDsISR(void* arg);
void readSensorDataISR(void* arg);
void sendSensorDataISR(void* arg);

void sendUUIDRequest(void);
bool initializeAdcChannels(void);

void IRAM_ATTR speedISR();
void IRAM_ATTR rpmISR();

/*
 *	Private Variables
 */
esp_timer_handle_t uuidTimerHandle_;
const esp_timer_create_args_t uuidTimerConf_ = {.callback = &requestUUIDsISR, .name = "Request HW UUIDs Timer"};
uint8_t uuidRequestCounter_;

bool operationModeInitialized_ = false;

wifi_config_t wifiConfig_;

esp_timer_handle_t readSensorDataTimerHandle_;
const esp_timer_create_args_t readSensorDataTimerConf_ = {.callback = &readSensorDataISR,
														  .name = "Read Sensor Data Timer"};

esp_timer_handle_t sendSensorDataTimerHandle_;
const esp_timer_create_args_t sendSensorDataTimerConf_ = {.callback = &sendSensorDataISR,
														  .name = "Send Sensor Data Timer"};

adc_oneshot_unit_handle_t adc1Handle_;
adc_oneshot_unit_handle_t adc2Handle_;

// Oil pressure stuff
bool oilPressure = false;
adc_cali_handle_t adc2OilCalibHandle_;

// Fuel level stuff
uint8_t fuelLevelInPercent = 0;
float fuelLevelResistance_ = 0.0f;
adc_cali_handle_t adc2FuelCalibHandle_;

// Water temperature stuff
uint8_t waterTemp = 0;
float waterTemperatureResistance_ = 0.0f;
adc_cali_handle_t adc2WaterCalibHandle_;

// Speed stuff
uint8_t vehicleSpeed = 0;
int64_t lastTimeOfFallingEdgeSpeed_ = 0;
int64_t timeOfFallingEdgeSpeed_ = 0;

// RPM stuff
uint16_t vehicleRPM = 0;
int rpmInHz_ = -1;
int64_t lastTimeOfFallingEdgeRPM_ = 0;
int64_t timeOfFallingEdgeRPM_ = 0;

// Internal temperature sensor stuff
int intTempRawAdcValue_ = 0;
int intTempVoltageMV_ = 0;
double internalTemperature_ = 0.0;
adc_cali_handle_t adc2IntTempCalibHandle_;

// Indicators
bool leftIndicator = false;
adc_cali_handle_t adc2LeftIndicatorHandle_;
bool rightIndicator = false;
adc_cali_handle_t adc2RightIndicatorHandle_;

/*
 *	Interrupt Service Routines
 */

//! \brief Connected to a Timer timeout to send UUID requests until we received all of them
void requestUUIDsISR(void* arg)
{
	// Create the event
	QUEUE_EVENT_T registerHwUUID;
	registerHwUUID.command = QUEUE_REQUEST_UUID;

	// Increase the timer
	uuidRequestCounter_++;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(mainEventQueue, &registerHwUUID, &xHigherPriorityTaskWoken);
}

//! \brief Connected to a Timer timeout to send new sensor data to the displays
void sendSensorDataISR(void* arg)
{
	// Create the event
	QUEUE_EVENT_T sendData;
	sendData.command = QUEUE_SEND_SENSOR_DATA;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(mainEventQueue, &sendData, &xHigherPriorityTaskWoken);
}

//! \brief Connected to a Timer timeout to read the data from all sensor
void readSensorDataISR(void* arg)
{
	// Create the event
	QUEUE_EVENT_T readData;
	readData.command = QUEUE_READ_SENSOR_DATA;

	// Queue it
	BaseType_t xHigherPriorityTaskWoken;
	xQueueSendFromISR(mainEventQueue, &readData, &xHigherPriorityTaskWoken);
}

//! \brief ISR for the speed, triggered everytime there is a falling edge
void speedISR()
{
	lastTimeOfFallingEdgeSpeed_ = timeOfFallingEdgeSpeed_;
	timeOfFallingEdgeSpeed_ = esp_timer_get_time();
}

//! \brief ISR for the rpm, triggered everytime there is a falling edge
void rpmISR()
{
	lastTimeOfFallingEdgeRPM_ = timeOfFallingEdgeRPM_;
	timeOfFallingEdgeRPM_ = esp_timer_get_time();
}

/*
 *	Main function
 */
void app_main(void)
{
	// Create the event queues
	createEventQueues();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Register the queue to the CAN bus
	registerMessageReceivedCbQueue(&mainEventQueue);

	// Send a UUID Request
	QUEUE_EVENT_T initRequest;
	initRequest.command = QUEUE_REQUEST_UUID;
	xQueueSend(mainEventQueue, &initRequest, portMAX_DELAY);

	// Wait for new queue events
	QUEUE_EVENT_T queueEvent;
	while (true) {
		// Wait until we get a new event in the queue
		if (xQueueReceive(mainEventQueue, &queueEvent, portMAX_DELAY)) {
			// Get current state
			const State_t currState = getCurrentState();

			// Act depending on the event
			switch (queueEvent.command) {
				/*
				 *	We received a CAN message
				 */
				case QUEUE_RECEIVED_NEW_CAN_MESSAGE:
					// Get the frame
					const twai_frame_t recFrame = queueEvent.canFrame;

					// Are we in the INIT state and is it an answer from one of the displays?
					if (currState == STATE_INIT && recFrame.buffer_len >= 1) {
						// Iterate through all UUIDs we already know
						for (int i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
							// Is the one we received one of them?
							if (knownHwUUIDs[i] == 0 || knownHwUUIDs[i] == recFrame.buffer[0]) {
								// Then save it
								knownHwUUIDs[i] = recFrame.buffer[0];

								// Create the buffer for the answer CAN frame
								uint8_t* buffer = malloc(sizeof(uint8_t) * 2);
								if (buffer == NULL) {
									break;
								}
								buffer[0] = knownHwUUIDs[i];
								buffer[1] = i;

								// Create the CAN answer frame
								twai_frame_t* frame = malloc(sizeof(twai_frame_t));
								memset(frame, 0, sizeof(*frame));
								frame->header.id = CAN_MSG_SET_ID;
								frame->header.dlc = 2;
								frame->header.ide = false;
								frame->header.rtr = false;
								frame->header.fdf = false;
								frame->buffer = buffer;
								frame->buffer_len = 2;

								// Send the frame
								queueCanBusMessage(frame, true, true);

								loggerInfo("Sending ID '%d' to UUID '%d'", buffer[1], buffer[0]);

								break;
							}
						}
					}
					break;


				/*
				 * Request the HW UUID of all devices
				 */
				case QUEUE_REQUEST_UUID:
					// Did we receive all HW UUIDs?
					bool allUUIDsReceived = true;
					for (int i = 0; i < sizeof(knownHwUUIDs); i++) {
						if (knownHwUUIDs[i] == 0) {
							// No
							allUUIDsReceived = false;
							break;
						}
					}

					// Do we need to re-request?
					if (!allUUIDsReceived) {
						// Send the request
						sendUUIDRequest();

						// And restart the timer
						const uint64_t timeout = uuidRequestCounter_ >= 10 ? 2000 * 1000 : 200 * 1000;
						if (uuidTimerHandle_ == NULL) {
							esp_timer_create(&uuidTimerConf_, &uuidTimerHandle_);
						}
						if (!esp_timer_is_active(uuidTimerHandle_)) {
							esp_timer_start_once(uuidTimerHandle_, timeout);
						}

						// Should we switch to the operation state?
						if (uuidRequestCounter_ >= 10 && currState != STATE_OPERATION) {
							setCurrentState(STATE_OPERATION);

							// Log it
							loggerInfo("Not all displays registered themselves within 2 Seconds");

							// Queue the init of the operation mode
							QUEUE_EVENT_T rq;
							rq.command = QUEUE_INIT_OPERATION_MODE;
							xQueueSend(mainEventQueue, &rq, pdMS_TO_TICKS(50));
						}
					}
					else {
						// Delete the timer
						esp_timer_delete(uuidTimerHandle_);
						uuidTimerHandle_ = NULL;

						// And then enter the OPERATION mode
						setCurrentState(STATE_OPERATION);

						// Queue the init of the operation mode
						QUEUE_EVENT_T rq;
						rq.command = QUEUE_INIT_OPERATION_MODE;
						xQueueSend(mainEventQueue, &rq, pdMS_TO_TICKS(50));
					}

					break;

				/*
				 *	Initialization of the OPERATION mode
				 */
				case QUEUE_INIT_OPERATION_MODE:
					// Are we already in the OPERATION mode but not yet initialized?
					if (currState == STATE_OPERATION && operationModeInitialized_ == false) {
						bool initSucceeded = true;

						/*
						 * Start the Wi-Fi
						 */
						// Initialize NVS
						initSucceeded &= nvs_flash_init() == ESP_OK;

						// Initialize TCP/IP
						initSucceeded &= esp_netif_init() == ESP_OK;
						initSucceeded &= esp_event_loop_create_default() == ESP_OK;

						// Initialize the AP mode
						esp_netif_create_default_wifi_ap();

						// Load the default config
						wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
						initSucceeded &= esp_wifi_init(&initConfig);

						// Register the Wi-Fi callbacks

						// Initialize the Wi-Fi config
						wifi_config_t wifiConfig = {.ap = {
														.ssid = "MX5-HybridDash Control Board",
														.ssid_len = strlen("MX5-HybridDash Control Board"),
														.channel = 1,
														.password = "unsafe",
														.max_connection = 4,
														.authmode = WIFI_AUTH_WPA2_PSK,
													}};
						wifiConfig_ = wifiConfig;

						// Start up the Wi-Fi
						initSucceeded &= esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK;
						initSucceeded &= esp_wifi_set_config(WIFI_IF_AP, &wifiConfig_) == ESP_OK;
						initSucceeded &= esp_wifi_start() == ESP_OK;

						/*
						 *	Start reading the sensor data to the displays
						 */
						if (readSensorDataTimerHandle_ == NULL) {
							initSucceeded &= esp_timer_create(&readSensorDataTimerConf_, &readSensorDataTimerHandle_);
						}
						if (!esp_timer_is_active(readSensorDataTimerHandle_)) {
							initSucceeded &= esp_timer_start_periodic(readSensorDataTimerHandle_,
																	  READ_SENSOR_DATA_INTERVAL_MS * 1000);
						}

						/*
						 *	Start sending the sensor data to the displays
						 */
						if (sendSensorDataTimerHandle_ == NULL) {
							initSucceeded &= esp_timer_create(&sendSensorDataTimerConf_, &sendSensorDataTimerHandle_);
						}
						if (!esp_timer_is_active(sendSensorDataTimerHandle_)) {
							initSucceeded &=
								esp_timer_start_once(sendSensorDataTimerHandle_, SEND_SENSOR_DATA_INTERVAL_MS * 1000);
						}

						/*
						 *	Initialize everything for reading from the sensors
						 */
						initSucceeded &= initializeAdcChannels();

						// Finished the initialization
						operationModeInitialized_ = initSucceeded;
					}

					break;


				/*
				 *	Read the sensor data
				 */
				case QUEUE_READ_SENSOR_DATA:
					break;

				/*
				 *	Send the sensor data
				 */
				case QUEUE_SEND_SENSOR_DATA:
					// Are we successfully initialize?
					if (!operationModeInitialized_) {
						break;
					}

					// Create the buffer for the answer CAN frame
					uint8_t* buffer = malloc(sizeof(uint8_t) * 8);
					if (buffer == NULL) {
						break;
					}
					buffer[0] = vehicleSpeed;
					buffer[1] = vehicleRPM >> 8;
					buffer[2] = (uint8_t)vehicleRPM;
					buffer[3] = fuelLevelInPercent;
					buffer[4] = waterTemp;
					buffer[5] = oilPressure;
					buffer[6] = leftIndicator;
					buffer[7] = rightIndicator;

					// Create the CAN answer frame
					twai_frame_t* frame = malloc(sizeof(twai_frame_t));
					memset(frame, 0, sizeof(*frame));
					frame->header.id = CAN_MSG_NEW_SENSOR_DATA;
					frame->header.dlc = 8;
					frame->header.ide = false;
					frame->header.rtr = false;
					frame->header.fdf = false;
					frame->buffer = buffer;
					frame->buffer_len = 8;

					// Send the frame
					queueCanBusMessage(frame, true, true);

					break;

				/*
				 *	Fallback
				 */
				default:
					break;
			}
		}
	}
}

/*
 *	Function Implementations
 */

void sendUUIDRequest(void)
{
	// Create the can frame
	twai_frame_t* frame = malloc(sizeof(twai_frame_t));
	memset(frame, 0, sizeof(*frame));
	frame->header.id = CAN_MSG_REQUEST_HW_UUID;
	frame->header.dlc = 1;
	frame->header.ide = false;
	frame->header.rtr = true;
	frame->header.fdf = false;

	// Send the frame
	queueCanBusMessage(frame, true, false);

	// Debug Logging
	loggerDebug("Sent HW UUID Request");
}

bool initializeAdcChannels(void)
{
	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc2InitConfig = {.unit_id = ADC_UNIT_2, .ulp_mode = ADC_ULP_MODE_DISABLE};
	if (adc_oneshot_new_unit(&adc2InitConfig, &adc2Handle_) != ESP_OK) {
		// Init was NOT successful!
		return false;

		// Logging
		loggerWarn("Failed to initialize ADC2!");

		// Initialization failed
		return 0;
	}

	/* --- Configure the oil pressure ADC2 channel --- */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_OIL_PRESSURE, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_OIL_PRESSURE, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2OilConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_2_5,
	};
	if (adc_oneshot_config_channel(adc2Handle_, ADC_CHANNEL_OIL_PRESSURE, &adc2OilConfig) != ESP_OK) {
		loggerError("Failed to initialize ADC2 channel: %d", ADC_CHANNEL_OIL_PRESSURE);
		return false;
	}

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t oilCalibConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_OIL_PRESSURE,
		.atten = ADC_ATTEN_DB_2_5,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&oilCalibConfig, &adc2OilCalibHandle_) != ESP_OK) {
		// Logging
		loggerError("'adc_cali_create_scheme_curve_fitting' for the oil pressure channel FAILED");
		return false;
	}

	/* --- Configure the oil pressure ADC2 channel --- */

	/* --- Configure the fuel level ADC2 channel --- */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_FUEL_LEVEL, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_FUEL_LEVEL, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2FuelConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_2_5,
	};
	if (adc_oneshot_config_channel(adc2Handle_, ADC_CHANNEL_FUEL_LEVEL, &adc2FuelConfig) != ESP_OK) {
		loggerError("Failed to initialize ADC2 channel: %d", ADC_CHANNEL_FUEL_LEVEL);
		return false;
	}

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t fuelCalibConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_FUEL_LEVEL,
		.atten = ADC_ATTEN_DB_2_5,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&fuelCalibConfig, &adc2FuelCalibHandle_) != ESP_OK) {
		// Logging
		loggerError("'adc_cali_create_scheme_curve_fitting' for the fuel level channel FAILED");
		return false;
	}

	/* --- Configure the fuel level ADC2 channel --- */

	/* --- Configure the water temperature ADC2 channel --- */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_WATER_TEMPERATURE, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_WATER_TEMPERATURE, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2WaterConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_12,
	};

	// Set config
	if (adc_oneshot_config_channel(adc2Handle_, ADC_CHANNEL_WATER_TEMPERATURE, &adc2WaterConfig) != ESP_OK) {
		loggerError("Failed to initialize ADC2 channel: %d", ADC_CHANNEL_WATER_TEMPERATURE);
		return false;
	}

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t waterCaliConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_WATER_TEMPERATURE,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&waterCaliConfig, &adc2WaterCalibHandle_) != ESP_OK) {
		// Logging
		loggerError("'adc_cali_create_scheme_curve_fitting' for the water temperature channel FAILED");

		return false;
	}

	/* --- Configure the water temperature ADC2 channel --- */

	/* --- Configure the speed interrupt --- */

	// Setup gpio
	gpio_set_direction(GPIO_SPEED, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_SPEED, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(GPIO_SPEED, GPIO_INTR_NEGEDGE);

	// Install ISR service
	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		// Logging
		loggerError("Couldn't install the ISR service. Speed and RPM are unavailable!");

		return false;
	}

	// Activate the ISR for measuring the frequency for the speed
	if (gpio_isr_handler_add(GPIO_SPEED, speedISR, NULL) != ESP_OK) {
		// Logging
		loggerError("Failed to enable the speed ISR!");

		return false;
	}

	/* --- Configure the speed interrupt --- */

	/* --- Configure the rpm interrupt --- */

	// Setup gpio
	gpio_set_direction(GPIO_RPM, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_RPM, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(GPIO_RPM, GPIO_INTR_NEGEDGE);

	// Activate the ISR for measuring the frequency for the rpm
	if (gpio_isr_handler_add(GPIO_RPM, rpmISR, NULL) != ESP_OK) {
		// Logging
		loggerError("Failed to enable the rpm ISR!");

		return false;
	}

	/* --- Configure the rpm interrupt --- */

	/* --- Configure the internal temperature sensor --- */

	// Setup gpio
	gpio_set_direction(GPIO_NUM_7, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_NUM_7, GPIO_PULLDOWN_ONLY);

	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc1InitConfig = {.unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE};
	if (adc_oneshot_new_unit(&adc1InitConfig, &adc1Handle_) != ESP_OK) {
		// Logging
		loggerWarn("Failed to initialize ADC1!");

		return false;
	}

	// Create the channel config
	const adc_oneshot_chan_cfg_t adc2IntTempConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_6,
	};

	// Set channel
	if (adc_oneshot_config_channel(adc1Handle_, ADC_CHANNEL_INT_TEMPERATURE, &adc2IntTempConfig) != ESP_OK) {
		loggerError("Failed to initialize ADC2 channel: %d", ADC_CHANNEL_INT_TEMPERATURE);

		return false;
	}

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t intTempCaliConfig = {
		.unit_id = ADC_UNIT_1,
		.chan = ADC_CHANNEL_INT_TEMPERATURE,
		.atten = ADC_ATTEN_DB_6,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&intTempCaliConfig, &adc2IntTempCalibHandle_) != ESP_OK) {
		// Logging
		loggerError("'adc_cali_create_scheme_curve_fitting' for the internal temperature sensor channel FAILED");

		return false;
	}

	/* --- Configure the internal temperature sensor --- */

	return true;
}
