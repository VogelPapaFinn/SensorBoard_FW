// Project includes
#include "Global.h"
#include "DataCenter.h"
#include "can.h"
#include "can_messages.h"
#include "logger.h"
#include "statemachine.h"

// espidf includes
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include "../../esp-idf/components/json/cJSON/cJSON.h"
#include "WebInterface.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 *	Defines
 */
// CAN
#define CAN_SENDER_ID 0x00

// Intervals
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

void sendUUIDRequest(void);
bool initializeAdcChannels(void);

void IRAM_ATTR speedISR();
void IRAM_ATTR rpmISR();

void convertSingleToDoubleQuote(char* str);

void debugListAllSpiffsFiles();

/*
 *	Private Variables
 */
esp_timer_handle_t uuidTimerHandle_;
const esp_timer_create_args_t uuidTimerConf_ = {.callback = &requestUUIDsISR, .name = "Request HW UUIDs Timer"};
uint8_t uuidRequestCounter_;

bool operationModeInitialized_ = false;

esp_timer_handle_t readSensorDataTimerHandle_;
const esp_timer_create_args_t readSensorDataTimerConf_ = {
	.callback = &readSensorDataISR,
	.name = "Read Sensor Data Timer"
};

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

	// Initialize the Data Center
	initDataCenter();

	// Initialize the can node
	twai_node_handle_t* canNodeHandle = initializeCanNode(GPIO_NUM_43, GPIO_NUM_2);
	enableCanNode();

	// Register the queue to the CAN bus
	registerCanRxCbQueue(&mainEventQueue);

	// Register the queue to the Data Center
	registerDataCenterCbQueue(&mainEventQueue);

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
				 *	Restart Display
				 */
				case QUEUE_RESTART_DISPLAY:
					// Do we have parameters?
					if (queueEvent.parameterLength <= 0) {
						loggerDebug("Not enough parameters");

						break;
					}

					// Create the can frame answer
					twai_frame_t* restartFrame = generateCanFrame(
						CAN_MSG_DISPLAY_RESTART, CAN_SENDER_ID, queueEvent.parameter, queueEvent.parameterLength);

					// Send the frame
					queueCanBusMessage(restartFrame, true, true);
					break;

				/*
				 *	We received a CAN message
				 */
				case QUEUE_RECEIVED_NEW_CAN_MESSAGE:
					loggerInfo("Received new can message");

					// Get the frame
					const twai_frame_t recFrame = queueEvent.canFrame;

					// Are we in the INIT state and is it an answer from one of the displays?
					if ((recFrame.header.id >> 21) == CAN_MSG_REGISTRATION && recFrame.buffer_len >= 6) {
						// Extract the UUID
						uint64_t uuid = 0x00;
						for (int i = 0; i < 6; i++) {
							uuid += recFrame.buffer[i] << (i * 8);
						}

						// Iterate through all UUIDs we already know
						for (int i = 0; i < AMOUNT_OF_DISPLAYS; i++) {
							// Is the one we received one of them?
							if (knownHwUUIDs[i] == 0 || knownHwUUIDs[i] == uuid) {
								// Then save it
								knownHwUUIDs[i] = uuid;

								// Create the buffer for the answer CAN frame
								uint8_t* buffer = malloc(sizeof(uint8_t) * 7);
								if (buffer == NULL) {
									break;
								}
								buffer[0] = recFrame.buffer[0];
								buffer[1] = recFrame.buffer[1];
								buffer[2] = recFrame.buffer[2];
								buffer[3] = recFrame.buffer[3];
								buffer[4] = recFrame.buffer[4];
								buffer[5] = recFrame.buffer[5];
								buffer[6] = i;

								// Create the can frame answer
								twai_frame_t* frame = generateCanFrame(
									CAN_MSG_COMID_ASSIGNATION, CAN_SENDER_ID, buffer, 7);

								// Send the frame
								queueCanBusMessage(frame, true, true);

								// Logging
								loggerInfo("Sending ID '%d' to UUID '%d-%d-%d-%d-%d-%d'", i, recFrame.buffer[5],
								           recFrame.buffer[4], recFrame.buffer[3], recFrame.buffer[2],
								           recFrame.buffer[1], recFrame.buffer[0]);

								// Build a formatted UUID
								char* formattedUUID = malloc(sizeof(char) * 24);
								snprintf(formattedUUID, 24, "%d-%d-%d-%d-%d-%d", recFrame.buffer[5], recFrame.buffer[4],
								         recFrame.buffer[3], recFrame.buffer[2], recFrame.buffer[1],
								         recFrame.buffer[0]);

								// Get all displays
								Display_t* displays = getDisplayStatiObjects();
								Display_t* display = NULL;

								// Check if the one that connected is already being tracked
								for (uint8_t j = 0; j < AMOUNT_OF_DISPLAYS; j++) {
									// Does the uuid match?
									if (displays[j].uuid != NULL && strcmp(displays[j].uuid, formattedUUID) == 0) {
										display = &displays[j];
										break;
									}
									// Nope and all following entries are default ones, so take this one
									else if (displays[j].uuid == NULL) {
										display = &displays[j];
										break;
									}
								}

								// Set its values
								display->connected = true;
								display->uuid = formattedUUID;
								display->comId = i;
								display->wifiStatus = malloc(strlen("Turned Off") + 1);
								strcpy(display->wifiStatus, "Turned Off");

								// Notify everybody that the display stati changed
								broadcastDisplayStatiChanged();

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
						// if (uuidRequestCounter_ >= 10 && currState != STATE_OPERATION) {
						//	setCurrentState(STATE_OPERATION);
						//
						//	// Log it
						//	loggerInfo("Not all displays registered themselves within 2 Seconds");
						//
						//	// Queue the init of the operation mode
						//	QUEUE_EVENT_T rq;
						//	rq.command = QUEUE_INIT_OPERATION_MODE;
						//	xQueueSend(mainEventQueue, &rq, pdMS_TO_TICKS(50));
						//}
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
						 * Start the Webinterface
						 */
						initSucceeded &= startWebInterface(GET_FROM_CONFIG);

						/*
						 *	Start reading the sensor data
						 */
						if (readSensorDataTimerHandle_ == NULL) {
							initSucceeded &= esp_timer_create(&readSensorDataTimerConf_, &readSensorDataTimerHandle_);
						}
						if (!esp_timer_is_active(readSensorDataTimerHandle_)) {
							initSucceeded &= esp_timer_start_periodic(readSensorDataTimerHandle_,
							                                          READ_SENSOR_DATA_INTERVAL_MS * 1000);
						}

						/*
						 *	Initialize everything for reading from the sensors
						 */
						initSucceeded &= initializeAdcChannels();

						// Finished the initialization
						operationModeInitialized_ = true;

						// List all files on the spiffs partition
						// debugListAllSpiffsFiles();
					}

					break;


				/*
				 *	Read the sensor data
				 */
				case QUEUE_READ_SENSOR_DATA:
					// broadcastSensorDataChanged();

					break;

				/*
				 *	Send the sensor data
				 */
				case QUEUE_SENSOR_DATA_CHANGED:
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
					twai_frame_t* sensorDataFrame = generateCanFrame(CAN_MSG_SENSOR_DATA, CAN_SENDER_ID, buffer, 8);

					// Send the frame
					queueCanBusMessage(sensorDataFrame, true, true);

					break;

				/*
				 *	Display Stati Changed
				 */
				case QUEUE_DISPLAY_STATI_CHANGED:
					// Get all displays
					char* jsonOutput = getAllDisplayStatiAsJSON();

					// Send the data to the webserver
					if (jsonOutput != NULL) {
						webinterfaceSendData(jsonOutput);
					}
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
	twai_frame_t* frame = generateCanFrame(CAN_MSG_REGISTRATION, CAN_SENDER_ID, NULL, 0);

	// Send the frame
	queueCanBusMessage(frame, true, false);

	// Debug Logging
	// loggerDebug("Sent HW UUID Request");
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

void convertSingleToDoubleQuote(char* str)
{
	// NULL check
	if (str == NULL) {
		return;
	}

	// Replace all ' with "
	while (*str) {
		if (*str == '\'') {
			*str = '\"';
			str++;
		}
	}
}

void debugListAllSpiffsFiles()
{
	DIR* dir = opendir("/resources"); // Muss mit base_path übereinstimmen
	if (dir == NULL) {
		loggerDebug("Fehler beim Öffnen des Verzeichnisses");
		return;
	}

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		// d_name ist der Dateiname
		loggerDebug("Gefundene Datei: %s", entry->d_name);

		// Vollen Pfad zusammenbauen
		char path[267];
		snprintf(path, sizeof(path), "/resources/%s", entry->d_name);

		// 3. Dateiinhalt lesen und ausgeben
		FILE* f = fopen(path, "r");
		if (f == NULL) {
			loggerDebug("Konnte Datei nicht öffnen");
			continue;
		}

		loggerDebug("%s", entry->d_name);
		// loggerDebug("--- Inhalt von %s ---", entry->d_name);
		// char line[128];
		// while (fgets(line, sizeof(line), f) != NULL) {
		// 	// Zeile ausgeben (ohne extra Newline, da fgets es behält)
		// 	printf("%s", line);
		// }
		// printf("\n"); // Abschluss-Newline für saubere Log-Ausgabe
		// loggerDebug("--- Ende von %s ---", entry->d_name);

		fclose(f);
	}
	closedir(dir);
}
