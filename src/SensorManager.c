#include "SensorManager.h"

// Project includes
#include "can.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

/*
 *	Private defines
 */
#define AMOUNT_OF_MANUAL_SENSORS 4
#define AMOUNT_OF_AUTOMATIC_SENSORS 4

#define MANUAL_SENSOR_READ_INTERVALL_HZ 10
#define SEND_SENSOR_DATA_INTERVAL_MS 100

#define GPIO_SPEED GPIO_NUM_14
#define GPIO_RPM GPIO_NUM_21

/*
 *	Prototypes
 */
//! \brief Function used as Task to periodically read the manual sensors
//! \param p_arg Unused pointer needed for FreeRTOS to accept the function as task
static void readSensorsTask(void* p_arg);

//! \brief Calibrates the specified sensors ADC channel
//! \param sensor The sensor which channel should be calibrated
//! \retval Boolean indicating if the calibration was applied successfully
static bool calibrateSensor(ManualReadSensor_t sensor);

//! \brief Removes the calibration from the specified sensors ADC channel
static void deleteCalibrationFromSensor(ManualReadSensor_t sensor);

//! \brief ISR which is triggered when a falling edge was recognized on the Speed GPIO
static void IRAM_ATTR speedISR();
//! \brief ISR which is triggered when a falling edge was recognized on the RPM GPIO
static void IRAM_ATTR rpmISR();
//! \brief ISR which is triggered when a new edge was recognized on the LINDICATOR GPIO
static void IRAM_ATTR lIndicatorISR();
//! \brief ISR which is triggered when a new edge was recognized on the RINDICATOR GPIO
static void IRAM_ATTR rIndicatorISR();

//! \brief ISR used to send all available sensor data periodically via CAN
//! \param p_arg Unused pointer needed for espidf so this can be used as timer ISR
static void sendSensorDataISR(void* p_arg);

/*
 *	Private variables
 */
//! \brief Boolean indicating if the initialization of the ADC2 was successful
static bool g_adc2Initialized = false;
//! \brief The handle of the ADC2
static adc_oneshot_unit_handle_t g_adc2Handle;
//! \brief The configuration which is used for all ADC1 and ADC2 channels
static adc_oneshot_chan_cfg_t g_adcChannelConfig = {
	.bitwidth = ADC_BITWIDTH_DEFAULT,
	.atten = ADC_ATTEN_DB_12,
};

//! \brief Map which contains the calibration handle for each manual sensor
static adc_cali_handle_t g_calibrationHandles[AMOUNT_OF_MANUAL_SENSORS];
//! \brief Map which contains the GPIO pin for each manual sensor
static const gpio_num_t g_manualSensorGPIOs[AMOUNT_OF_MANUAL_SENSORS] = {
	[OIL_PRESSURE] = GPIO_NUM_12,
	[FUEL_LEVEL] = GPIO_NUM_11,
	[WATER_TEMP] = GPIO_NUM_13,
	[INTERNAL_TEMP] = GPIO_NUM_7,
};
//! \brief Map which contains the ADC2 channel for each manual sensor
static const gpio_num_t g_manualSensorAdcChannels[AMOUNT_OF_MANUAL_SENSORS] = {
	[OIL_PRESSURE] = ADC_CHANNEL_1,
	[FUEL_LEVEL] = ADC_CHANNEL_0,
	[WATER_TEMP] = ADC_CHANNEL_2,
	[INTERNAL_TEMP] = ADC_CHANNEL_6,
};
//! \brief Map which contains booleans indicating if the manual sensors are being currently read
static bool g_manualSensorsReadingStatus[AMOUNT_OF_MANUAL_SENSORS];

//! \brief Boolean indicating if the initialization of the ADC1 was successful
static bool g_adc1Initialized = false;
//! \brief The handle of the ADC1
static adc_oneshot_unit_handle_t g_adc1Handle;

//! \brief Bool indicating if the ISR service was installed successfully. If not all
//! automatic sensors are NOT available
static bool g_isrServiceInstalled = false;
//! \brief Map which contains the GPIO pin for each automatic sensor
static const gpio_num_t g_automaticSensorGPIOs[AMOUNT_OF_AUTOMATIC_SENSORS] = {
	[SPEED] = GPIO_NUM_3,
	[RPM] = GPIO_NUM_21,
	[L_INDICATOR] = GPIO_NUM_10,
	[R_INDICATOR] = GPIO_NUM_9,
};
//! \brief Map which contains the edge on which the ISR for each automatic sensor is triggered
static const gpio_int_type_t g_automaticSensorEdgeTrigger[AMOUNT_OF_AUTOMATIC_SENSORS] = {
	[SPEED] = GPIO_INTR_NEGEDGE,
	[RPM] = GPIO_INTR_NEGEDGE,
	[L_INDICATOR] = GPIO_INTR_ANYEDGE,
	[R_INDICATOR] = GPIO_INTR_ANYEDGE,
};
//! \brief Map which contains the ISR function for each automatic sensor
static const gpio_isr_t g_automaticSensorISR[AMOUNT_OF_AUTOMATIC_SENSORS] = {
	[SPEED] = speedISR,
	[RPM] = rpmISR,
	[L_INDICATOR] = lIndicatorISR,
	[R_INDICATOR] = rIndicatorISR,
};
//! \brief Map which contains booleans indicating if the automatic sensor ISRs are currently active
static bool g_automaticSensorsReadingStatus[AMOUNT_OF_AUTOMATIC_SENSORS];

//! \brief Handle of timer which is used to periodically send all available sensor data via CAN
static esp_timer_handle_t g_sendSensorDataTimerHandle;
//! \brief The configuration of the timer which is used to periodically send all available sensor data via CAN
static const esp_timer_create_args_t g_sendSensorDataTimerConf = {.callback = &sendSensorDataISR,
																  .name = "Send Sensor Data Timer"};

//! \brief Integer containing the last time a falling edge on the speed GPIO was registered
static int64_t g_lastTimeOfFallingEdgeSpeed = 0;
//! \brief Integer containing the current time a falling edge on the speed GPIO was registered
static int64_t g_timeOfFallingEdgeSpeed = 0;

//! \brief Integer containing the last time a falling edge on the rpm GPIO was registered
static int64_t g_lastTimeOfFallingEdgeRPM = 0;
//! \brief Integer containing the current time a falling edge on the rpm GPIO was registered
static int64_t g_timeOfFallingEdgeRPM = 0;

//! \brief 1 Byte integer representing the vehicle speed in kmh
static uint8_t g_vehicleSpeed;
//! \brief 2 Byte integer representing the rpm
static uint16_t g_vehicleRPM;
//! \brief 1 Byte integer representing the fuel level in percent
static uint8_t g_fuelLevelInPercent;
//! \brief 1 Byte integer representing the water temperature
static uint8_t g_waterTemp;
//! \brief Bool indicating if oil pressure is high enough
static bool g_oilPressure;
//! \brief Bool indicating if the left indicator is active
static bool g_isLeftIndicatorActive = false;
//! \brief Bool indicating if the right indicator is active
static bool g_isRightIndicatorActive = false;

/*
 *	Private functions
*/
static void readSensorsTask(void* p_arg)
{
	if (!g_adc2Initialized) {
		return;
	}

	// Variables we save each adc poll into
	int oilPressure, fuelLevel, waterTemp, internalTemp;

	// Read sensors using EMA
	// EMA -> CurrentValue = (NewlyReadValue * Alpha) + (CurrentValue * (1 - Alpha))
	while (true) {
		// Read Oil Pressure
		if (g_manualSensorsReadingStatus[OIL_PRESSURE]) {

		}

		// Read Fuel Level
		if (g_manualSensorsReadingStatus[FUEL_LEVEL]) {

		}

		// Read Water Temp
		if (g_manualSensorsReadingStatus[WATER_TEMP]) {

		}

		// Read Internal Temp
		if (g_manualSensorsReadingStatus[INTERNAL_TEMP]) {

		}

		// Sleep for the specified length
		vTaskDelay(pdMS_TO_TICKS((1 / MANUAL_SENSOR_READ_INTERVALL_HZ) * 1000)); // HZ to MS
	}
}

static bool calibrateSensor(const ManualReadSensor_t sensor)
{
	// Apply curve calibration
	const adc_cali_curve_fitting_config_t curveCalibrationConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = g_manualSensorAdcChannels[sensor],
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	if (adc_cali_create_scheme_curve_fitting(&curveCalibrationConfig, &g_calibrationHandles[sensor]) != ESP_OK) {
		return false;
	}

	return true;
}

static void deleteCalibrationFromSensor(const ManualReadSensor_t sensor)
{
	adc_cali_delete_scheme_curve_fitting(g_calibrationHandles[sensor]);
}

bool startReadingManualSensor(const ManualReadSensor_t sensor)
{
	if (!g_adc2Initialized || g_manualSensorsReadingStatus[sensor]) {
		return false;
	}

	// Set up the GPIO
	gpio_set_direction(g_manualSensorGPIOs[sensor], GPIO_MODE_INPUT);
	gpio_set_pull_mode(g_manualSensorGPIOs[sensor], GPIO_PULLDOWN_ONLY);

	// Configure the channel
	if (adc_oneshot_config_channel(g_adc2Handle, g_manualSensorAdcChannels[sensor], &g_adcChannelConfig) != ESP_OK) {
		ESP_LOGE("SensorManager", "Failed to start reading manual sensor '%d'!", sensor);

		return false;
	}

	// Apply the calibration
	if (!calibrateSensor(sensor)) {
		ESP_LOGE("SensorManager", "Failed to apply calibration to manual sensor '%d'!", sensor);

		return false;
	}

	// Start reading the sensor
	g_manualSensorsReadingStatus[sensor] = true;
	return true;
}

void stopReadingManualSensor(const ManualReadSensor_t sensor)
{
	if (!g_adc2Initialized || !g_manualSensorsReadingStatus[sensor]) {
		return;
	}

	// Disable reading
	g_manualSensorsReadingStatus[sensor] = false;

	// Remove the calibration from the channel
	deleteCalibrationFromSensor(sensor);
}

bool startReadingAutomaticSensor(const AutomaticReadSensor_t sensor)
{
	if (!g_isrServiceInstalled) {
		return false;
	}

	// Setup gpio
	gpio_set_direction(g_automaticSensorGPIOs[sensor], GPIO_MODE_INPUT);
	gpio_set_pull_mode(g_automaticSensorGPIOs[sensor], GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(g_automaticSensorGPIOs[sensor], g_automaticSensorEdgeTrigger[sensor]);

	// Activate the ISR
	if (gpio_isr_handler_add(g_automaticSensorGPIOs[sensor], g_automaticSensorISR[sensor], NULL) == ESP_OK) {
		// Everything worked
		g_automaticSensorsReadingStatus[sensor] = true;
	}
	else {
		// It failed
		g_automaticSensorsReadingStatus[sensor] = false;

		// Logging
		ESP_LOGE("SensorManager", "Failed to enable the ISR for automatic sensor '%d'!", sensor);
		return false;
	}

	return true;
}

bool stopReadingAutomaticSensor(const AutomaticReadSensor_t sensor)
{
	if (!g_isrServiceInstalled) {
		return true;
	}

	// Deactivate the ISR
	if (gpio_isr_handler_remove(g_automaticSensorGPIOs[sensor]) == ESP_OK) {
		// Everything worked
		g_automaticSensorsReadingStatus[sensor] = false;

		// Reset gpio
		gpio_set_direction(g_automaticSensorGPIOs[sensor], GPIO_MODE_DISABLE);

		return true;
	}

	// It failed
	g_automaticSensorsReadingStatus[sensor] = true;
	return false;
}

/*
 *	Private ISR's
 */
static void speedISR()
{
	g_lastTimeOfFallingEdgeSpeed = g_timeOfFallingEdgeSpeed;
	g_timeOfFallingEdgeSpeed = esp_timer_get_time();
}

static void rpmISR()
{
	g_lastTimeOfFallingEdgeRPM = g_timeOfFallingEdgeRPM;
	g_timeOfFallingEdgeRPM = esp_timer_get_time();
}

static void lIndicatorISR()
{
	g_isLeftIndicatorActive = gpio_get_level(g_automaticSensorGPIOs[L_INDICATOR]);
}

static void rIndicatorISR()
{
	g_isRightIndicatorActive = gpio_get_level(g_automaticSensorGPIOs[R_INDICATOR]);
}

static void sendSensorDataISR(void* p_arg)
{
	// Create the buffer for the answer CAN frame
	uint8_t* buffer = malloc(sizeof(uint8_t) * 8);
	if (buffer == NULL) {
		return;
	}
	buffer[0] = g_vehicleSpeed;
	buffer[1] = g_vehicleRPM >> 8;
	buffer[2] = (uint8_t)g_vehicleRPM;
	buffer[3] = g_fuelLevelInPercent;
	buffer[4] = g_waterTemp;
	buffer[5] = g_oilPressure;
	buffer[6] = g_isLeftIndicatorActive;
	buffer[7] = g_isRightIndicatorActive;

	// Create the CAN answer frame
	twai_frame_t* sensorDataFrame = generateCanFrame(CAN_MSG_SENSOR_DATA, g_ownCanComId, &buffer, 8);

	// Send the frame
	queueCanBusMessage(sensorDataFrame, true, true);
}

/*
 *	Public function implementations
*/
void sensorManagerInit()
{
	// Initialize the ADC1
	const adc_oneshot_unit_init_cfg_t adc1InitConfig = {
		.unit_id = ADC_UNIT_1,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc1InitConfig, &g_adc1Handle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Failed to initialize ADC1!");

		return;
	}

	// Initialization successful
	g_adc1Initialized = true;

	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc2InitConfig = {
		.unit_id = ADC_UNIT_2,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc2InitConfig, &g_adc2Handle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Failed to initialize ADC2!");

		return;
	}

	// Initialization successful
	g_adc2Initialized = true;

	// Install ISR service for
	g_isrServiceInstalled = true;
	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Couldn't install the ISR service. Speed, RPM and Indicators are unavailable!");

		g_isrServiceInstalled = false;
	}

	// Start the read manual sensor task
	xTaskCreate(&readSensorsTask, "readSensorsTask", 1024, NULL, 0, NULL);
}

bool sensorManagerStartReadingAllSensors()
{
	bool success = true;

	// Start the manual sensors
	success &= startReadingManualSensor(OIL_PRESSURE);
	success &= startReadingManualSensor(FUEL_LEVEL);
	success &= startReadingManualSensor(WATER_TEMP);
	success &= startReadingManualSensor(INTERNAL_TEMP);

	// Start the automatic sensors
	success &= startReadingAutomaticSensor(SPEED);
	success &= startReadingAutomaticSensor(RPM);
	success &= startReadingAutomaticSensor(L_INDICATOR);
	success &= startReadingAutomaticSensor(R_INDICATOR);

	return success;
}

bool sensorManagerStopReadingAllSensors()
{
	// Stop the manual sensors
	stopReadingManualSensor(OIL_PRESSURE);
	stopReadingManualSensor(FUEL_LEVEL);
	stopReadingManualSensor(WATER_TEMP);
	stopReadingManualSensor(INTERNAL_TEMP);

	// Stop the automatic sensors
	bool success = true;
	success &= stopReadingAutomaticSensor(SPEED);
	success &= stopReadingAutomaticSensor(RPM);
	success &= stopReadingAutomaticSensor(L_INDICATOR);
	success &= stopReadingAutomaticSensor(R_INDICATOR);

	return success;
}

bool sensorManagerStartSendingSensorData()
{
	if (g_sendSensorDataTimerHandle == NULL) {
		esp_timer_create(&g_sendSensorDataTimerConf, &g_sendSensorDataTimerHandle);
	}
	if (!esp_timer_is_active(g_sendSensorDataTimerHandle)) {
		return esp_timer_start_periodic(g_sendSensorDataTimerHandle, (uint64_t)(SEND_SENSOR_DATA_INTERVAL_MS * 1000)) == ESP_OK; // NOLINT
	}

	return false;
}

void sensorManagerStopSendingSensorData()
{
	if (g_sendSensorDataTimerHandle == NULL) {
		esp_timer_stop(g_sendSensorDataTimerHandle);
		esp_timer_delete(g_sendSensorDataTimerHandle);
		g_sendSensorDataTimerHandle = NULL;
	}
}

bool sensorManagerStartReadingManualSensor(const ManualReadSensor_t sensor)
{
	switch (sensor) {
		case OIL_PRESSURE:
			return startReadingManualSensor(OIL_PRESSURE);
		case FUEL_LEVEL:
			return startReadingManualSensor(FUEL_LEVEL);
		case WATER_TEMP:
			return startReadingManualSensor(WATER_TEMP);
		case INTERNAL_TEMP:
			return startReadingManualSensor(INTERNAL_TEMP);
		default:
			ESP_LOGW("SensorManager", "Received faulty sensor which should be read: '%d'", sensor);
			return false;
	};
}

void sensorManagerStopReadingManualSensor(const ManualReadSensor_t sensor)
{
	switch (sensor) {
		case OIL_PRESSURE:
			stopReadingManualSensor(OIL_PRESSURE);
			break;
		case FUEL_LEVEL:
			stopReadingManualSensor(FUEL_LEVEL);
			break;
		case WATER_TEMP:
			stopReadingManualSensor(WATER_TEMP);
			break;
		case INTERNAL_TEMP:
			stopReadingManualSensor(INTERNAL_TEMP);
			break;
		default:
			ESP_LOGW("SensorManager", "Received faulty sensor which should no longer be read: '%d'", sensor);
	};
}

bool sensorManagerStartReadingAutomaticSensor(const AutomaticReadSensor_t sensor)
{
	switch (sensor) {
		case SPEED:
		case RPM:
		case L_INDICATOR:
			return startReadingAutomaticSensor(sensor);
		default:
			ESP_LOGW("SensorManager", "Received faulty automatic sensor which should be activated: '%d'", sensor);
			return false;
	}
}

void sensorManagerStopReadingAutomaticSensor(const AutomaticReadSensor_t sensor)
{
	switch (sensor) {
		case SPEED:
		case RPM:
		case L_INDICATOR:
			stopReadingAutomaticSensor(sensor);
			break;
		default:
			ESP_LOGW("SensorManager", "Received faulty automatic sensor which should be deactivated: '%d'", sensor);
	}
}
