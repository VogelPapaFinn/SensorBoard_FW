#include "SensorManager.h"

// C includes
#include <math.h>

// espidf includes
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_timer.h>
#include <esp_log.h>


#define READ_SENSOR_DATA_INTERVAL_MS 50
#define SEND_SENSOR_DATA_INTERVAL_MS 100

// The ADC handles
adc_oneshot_unit_handle_t g_adc1Handle;
adc_oneshot_unit_handle_t g_adc2Handle;

// Where the ADC's correctly initialized
bool g_initAdc2Failed = false;

// Oil pressure stuff
bool g_oilPressure = false;
adc_cali_handle_t g_adc2OilCalibHandle;

// Fuel level stuff
uint8_t g_fuelLevelInPercent = 0;
float g_fuelLevelResistance = 0.0f;
adc_cali_handle_t g_adc2FuelCalibHandle;

// Water temperature stuff
uint8_t g_waterTemp = 0;
float g_waterTemperatureResistance = 0.0f;
adc_cali_handle_t g_adc2WaterCalibHandle;

// Speed stuff
static int g_speedInHz = -1;
uint8_t g_vehicleSpeed = 0;
int64_t g_lastTimeOfFallingEdgeSpeed = 0;
int64_t g_timeOfFallingEdgeSpeed = 0;
static bool g_speedIsrActive = false;

// RPM stuff
uint16_t g_vehicleRPM = 0;
int g_rpmInHz = -1;
int64_t g_lastTimeOfFallingEdgeRPM = 0;
int64_t g_timeOfFallingEdgeRPM = 0;
static bool g_rpmIsrActive = false;

// Internal temperature sensor stuff
int g_intTempRawAdcValue = 0;
int g_intTempVoltageMV = 0;
double g_internalTemperature = 0.0;
adc_cali_handle_t g_adc2IntTempCalibHandle;

// Indicators
bool g_leftIndicator = false;
adc_cali_handle_t g_adc2LeftIndicatorHandle;
bool g_rightIndicator = false;
adc_cali_handle_t g_adc2RightIndicatorHandle;

#include "EventQueues.h"
//! \brief Connected to a Timer timeout to read the data from all sensor
void readSensorDataISR(void* p_arg)
{
	sensorManagerReadAllSensors();
}
static esp_timer_handle_t g_readSensorDataTimerHandle;
static const esp_timer_create_args_t g_readSensorDataTimerConf = {.callback = &readSensorDataISR,
																  .name = "Read Sensor Data Timer"};

#include "can.h"
void sendSensorDataISR(void* p_arg)
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
	buffer[6] = g_leftIndicator;
	buffer[7] = g_rightIndicator;

	// Create the CAN answer frame
	twai_frame_t* sensorDataFrame = generateCanFrame(CAN_MSG_SENSOR_DATA, g_ownCanSenderId, &buffer, 8);

	// Send the frame
	queueCanBusMessage(sensorDataFrame, true, true);
}
static esp_timer_handle_t g_sendSensorDataTimerHandle;
static const esp_timer_create_args_t g_sendSensorDataTimerConf = {.callback = &sendSensorDataISR,
																  .name = "Send Sensor Data Timer"};


/*
 *	ISR's
 */
//! \brief ISR for the speed, triggered everytime there is a falling edge
static void IRAM_ATTR speedInterruptHandler()
{
	g_lastTimeOfFallingEdgeSpeed = g_timeOfFallingEdgeSpeed;
	g_timeOfFallingEdgeSpeed = esp_timer_get_time();
}

//! \brief ISR for the rpm, triggered everytime there is a falling edge
static void IRAM_ATTR rpmInterruptHandler()
{
	g_lastTimeOfFallingEdgeRPM = g_timeOfFallingEdgeRPM;
	g_timeOfFallingEdgeRPM = esp_timer_get_time();
}


/*
 *	Private Functions
 */

//! \brief Calculates the resistance of a voltage divider
//! \param preR1VoltageV The original voltage the divider works with [in Volts] e.g. 3.3V
//! \param voltageMV The measured voltage between R1 and R2 [in Millivolts]
//! \param r1 The resistance of R1
//! \retval The calculated resistance of R2 as float
float calculateVoltageDividerR2(const float preR1VoltageV, const int voltageMV, const int r1)
{
	const float vIn = preR1VoltageV;
	const float vOut = (float)voltageMV / 1000.0f;
	const float r = (float)r1;

	// R2 = R1 * (voltageMV / (preR1VoltageV - voltageMV))
	return r * (vOut / (vIn - vOut));
}

//! \brief Calculates the fuel level in PERCENT from the measured R2 resistance.
//! It uses a non-linear function as the fuel level sensor output is not proportional
//! to the fuel level.
//! \retval The fuel level in PERCENT as int
//! TODO: Implement the non-linear function!
int calculateFuelLevelFromResistance()
{
	float resistance = g_fuelLevelResistance;

	// Remove the resistance offset
	resistance -= FUEL_LEVEL_OFFSET;

	// Check if its < 0
	if (resistance < 0.0f) resistance = 0.0f;

	// Check if its > FUEL_LEVEL_TO_PERCENTAGE - FUEL_LEVEL_OFFSET
	if (resistance > FUEL_LEVEL_TO_PERCENTAGE - FUEL_LEVEL_OFFSET)
		resistance = FUEL_LEVEL_TO_PERCENTAGE -
			FUEL_LEVEL_OFFSET;

	// Then convert it to percent and return it
	return (int)((resistance / FUEL_LEVEL_TO_PERCENTAGE) * 100.0f);
}

//! \brief Calculates the water temperature in degree Celsius from the measured R2 resistance.
//! It uses a non-linear function as the water temp sensor output is not proportional
//! to the water temperature.
//! \retval The water temperature in degree Celsius as float
//! TODO: Implement the non-linear function!
float calculateWaterTemperatureFromResistance()
{
	float resistance = g_waterTemperatureResistance;

	// Remove the resistance offset
	resistance -= FUEL_LEVEL_OFFSET;

	// Check if its < 0
	if (resistance < 0.0f) resistance = 0.0f;

	// Then convert it to percent and return it
	return (resistance / FUEL_LEVEL_TO_PERCENTAGE * 100.0f);
}

//! \brief Calculates the speed in kmh from the measured frequency.
//! \retval The speed in kmh
//! TODO: Implement actual conversion
float calculateSpeedFromFrequency()
{
	return (float)g_speedInHz;
}

//! \brief Calculates the rpm from the measured frequency.
//! \retval The rpm's
int calculateRpmFromFrequency()
{
	double multiplier = 0.0;

	// Get the multiplier
	if (g_rpmInHz <= 0)
		return -1;
	if (g_rpmInHz <= 8)
		multiplier = 50.0;
	else if (g_rpmInHz <= 11)
		multiplier = 45.45;
	else if (g_rpmInHz <= 17)
		multiplier = 41.18;
	else if (g_rpmInHz <= 25)
		multiplier = 40.0;
	else if (g_rpmInHz <= 56)
		multiplier = 34.48;
	else if (g_rpmInHz <= 92)
		multiplier = 32.61;
	else if (g_rpmInHz <= 123)
		multiplier = 32.52;
	else if (g_rpmInHz <= 157)
		multiplier = 31.85;
	else if (g_rpmInHz <= 188)
		multiplier = 31.91;
	else if (g_rpmInHz <= 220)
		multiplier = 31.82;
	else if (g_rpmInHz <= 262)
		multiplier = 30.54;

	// Calculate the rpm and return it
	return (int)((double)g_rpmInHz * multiplier);
}

/*
 *	Function implementations
 */
bool sensorManagerInit(void)
{
	bool success = true;

	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc2InitConfig = {
		.unit_id = ADC_UNIT_2,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc2InitConfig, &g_adc2Handle) != ESP_OK) {
		// Init was NOT successful!
		g_initAdc2Failed = true;

		// Logging
		ESP_LOGW("SensorManager", "Failed to initialize ADC2!");

		// Initialization failed
		return false;
	}

	/*
	 *	Configure the oil pressure ADC2 channel
	 */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_OIL_PRESSURE, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_OIL_PRESSURE, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2OilConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_2_5,
	};
	success &= adc_oneshot_config_channel(g_adc2Handle, ADC_CHANNEL_OIL_PRESSURE, &adc2OilConfig) ==
		ESP_OK;

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t oilCaliConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_OIL_PRESSURE,
		.atten = ADC_ATTEN_DB_2_5,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&oilCaliConfig, &g_adc2OilCalibHandle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "'adc_cali_create_scheme_curve_fitting' for the oil pressure channel FAILED");
	}

	/*
	 *	Configure the fuel level ADC2 channel
	 */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_FUEL_LEVEL, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_FUEL_LEVEL, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2FuelConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_2_5,
	};
	success &= adc_oneshot_config_channel(g_adc2Handle, ADC_CHANNEL_FUEL_LEVEL, &adc2FuelConfig) ==
		ESP_OK;

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t fuelCaliConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_FUEL_LEVEL,
		.atten = ADC_ATTEN_DB_2_5,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&fuelCaliConfig, &g_adc2FuelCalibHandle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "'adc_cali_create_scheme_curve_fitting' for the fuel level channel FAILED");
	}

	/*
	 *	Configure the water temperature ADC2 channel
	 */

	// Configure oil pressure GPIO
	gpio_set_direction(GPIO_WATER_TEMPERATURE, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_WATER_TEMPERATURE, GPIO_PULLDOWN_ONLY);

	// Create the config
	const adc_oneshot_chan_cfg_t adc2WaterConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_12,
	};
	success &= adc_oneshot_config_channel(g_adc2Handle, ADC_CHANNEL_WATER_TEMPERATURE,
	                                      &adc2WaterConfig) == ESP_OK;

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t waterCaliConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = ADC_CHANNEL_WATER_TEMPERATURE,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&waterCaliConfig, &g_adc2WaterCalibHandle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "'adc_cali_create_scheme_curve_fitting' for the water temperature channel FAILED");
	}

	/*
	 *	Configure the speed interrupt
	 */

	// Setup gpio
	gpio_set_direction(GPIO_SPEED, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_SPEED, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(GPIO_SPEED, GPIO_INTR_NEGEDGE);

	// Install ISR service
	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Couldn't install the ISR service. Speed and RPM are unavailable!");
	}

	// Activate the ISR for measuring the frequency for the speed
	if (sensorManagerEnableSpeedISR()) {
		// Everything worked
		g_speedIsrActive = true;
	}
	else {
		// It failed
		g_speedIsrActive = false;

		// Logging
		ESP_LOGE("SensorManager", "Failed to enable the speed ISR!");
	}

	/*
	 *	Configure the rpm interrupt
	 */

	// Setup gpio
	gpio_set_direction(GPIO_RPM, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_RPM, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(GPIO_RPM, GPIO_INTR_NEGEDGE);

	// Activate the ISR for measuring the frequency for the rpm
	if (sensorManagerEnableRpmISR()) {
		// Everything worked
		g_rpmIsrActive = true;
	}
	else {
		// It failed
		g_rpmIsrActive = false;

		// Logging
		ESP_LOGE("SensorManager", "Failed to enable the rpm ISR!");
	}

	/*
	 *	Configure the internal temperature sensor
	 */

	// Setup gpio
	gpio_set_direction(GPIO_NUM_7, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_NUM_7, GPIO_PULLDOWN_ONLY);

	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc1InitConfig = {
		.unit_id = ADC_UNIT_1,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc1InitConfig, &g_adc1Handle) != ESP_OK) {
		// Logging
		ESP_LOGW("SensorManager", "Failed to initialize ADC1!");
	}

	// Create the channel config
	const adc_oneshot_chan_cfg_t adc2IntTempConfig = {
		.bitwidth = ADC_BITWIDTH_12,
		.atten = ADC_ATTEN_DB_6,
	};
	success &= adc_oneshot_config_channel(g_adc1Handle, ADC_CHANNEL_INT_TEMPERATURE,
	                                      &adc2IntTempConfig) == ESP_OK;

	// Create the calibration curve config
	const adc_cali_curve_fitting_config_t intTempCaliConfig = {
		.unit_id = ADC_UNIT_1,
		.chan = ADC_CHANNEL_INT_TEMPERATURE,
		.atten = ADC_ATTEN_DB_6,
		.bitwidth = ADC_BITWIDTH_12,
	};

	// Create calibration curve fitting
	if (adc_cali_create_scheme_curve_fitting(&intTempCaliConfig, &g_adc2IntTempCalibHandle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "'adc_cali_create_scheme_curve_fitting' for the internal temperature sensor channel FAILED");
	}

	return success;
}

bool sensorManagerUpdateOilPressure(void)
{
	// Was the init successfully?
	if (g_initAdc2Failed) return false;

	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(g_adc2Handle, ADC_CHANNEL_OIL_PRESSURE, &rawAdcValue) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to read the oil pressure from the ADC!");
	}

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adc2OilCalibHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to calculate the voltage from the ADC value!");
	}

	// Check the thresholds
	const bool oldOilPressureValue = g_oilPressure;
	g_oilPressure = voltage > OIL_LOWER_VOLTAGE_THRESHOLD && voltage < OIL_UPPER_VOLTAGE_THRESHOLD;

	// Did it change?
	if (oldOilPressureValue != g_oilPressure) {
		// Logging
		ESP_LOGD("SensorManager", "Oil pressure changed! From: '%d' to '%d'", oldOilPressureValue, g_oilPressure);

		return true;
	}
	return false;
}

void sensorManagerReadAllSensors()
{
	sensorManagerUpdateFuelLevel();
	sensorManagerUpdateInternalTemperature();
	sensorManagerUpdateOilPressure();
	sensorManagerUpdateRPM();
	sensorManagerUpdateSpeed();
	sensorManagerUpdateSpeed();
}

bool sensorManagerUpdateFuelLevel(void)
{
	// Was the init successfully?
	if (g_initAdc2Failed) return false;

	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(g_adc2Handle, ADC_CHANNEL_FUEL_LEVEL, &rawAdcValue) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to read the fuel level from the ADC!");
	}

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adc2FuelCalibHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to calculate the voltage from the ADC value!");
	}

	// Calculate resistance
	g_fuelLevelResistance = calculateVoltageDividerR2(OIL_FUEL_WATER_VOLTAGE_V, voltage, OIL_FUEL_R1);

	// Calculate the fuel level from the calculated resistance
	const int oldFuelLevelValue = g_fuelLevelInPercent;
	g_fuelLevelInPercent = calculateFuelLevelFromResistance();

	// Did it change?
	if (oldFuelLevelValue != g_fuelLevelInPercent) {
		// Logging
		ESP_LOGD("SensorManager", "Fuel level changed! From: '%d percent' to '%d percent'", oldFuelLevelValue, g_fuelLevelInPercent);

		return true;
	}
	return false;
}

bool sensorManagerUpdateWaterTemperature(void)
{
	// Was the init successfully?
	if (g_initAdc2Failed) return false;

	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(g_adc2Handle, ADC_CHANNEL_WATER_TEMPERATURE, &rawAdcValue) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to read the water temperature from the ADC!");
	}

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adc2WaterCalibHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("SensorManager", "Failed to calculate the voltage from the ADC value!");
	}

	// Calculate resistance
	g_waterTemperatureResistance = calculateVoltageDividerR2(OIL_FUEL_WATER_VOLTAGE_V, voltage, WATER_R1);

	// Calculate the water temperature from the calculated resistance
	const float oldWaterTemperatureValue = g_waterTemp;
	g_waterTemp = (uint8_t)calculateWaterTemperatureFromResistance();

	// Did it change?
	if (oldWaterTemperatureValue != (float)g_waterTemp) {
		// Logging
		ESP_LOGD("SensorManager", "Water temperature changed! From: '%d °C' to '%d °C'", oldWaterTemperatureValue, g_waterTemp);

		return true;
	}
	return false;
}

bool sensorManagerEnableSpeedISR()
{
	ESP_LOGI("SensorManager", "sensorManagerEnableSpeedISR");
	if (g_readSensorDataTimerHandle == NULL) {
		esp_timer_create(&g_readSensorDataTimerConf, &g_readSensorDataTimerHandle);
	}
	if (!esp_timer_is_active(g_readSensorDataTimerHandle)) {
		esp_timer_start_periodic(g_readSensorDataTimerHandle, READ_SENSOR_DATA_INTERVAL_MS * 1000);
	}

	if (g_sendSensorDataTimerHandle == NULL) {
		esp_timer_create(&g_sendSensorDataTimerConf, &g_sendSensorDataTimerHandle);
	}
	if (!esp_timer_is_active(g_sendSensorDataTimerHandle)) {
		esp_timer_start_periodic(g_sendSensorDataTimerHandle, SEND_SENSOR_DATA_INTERVAL_MS * 1000);
	}

	return (gpio_isr_handler_add(GPIO_SPEED, speedInterruptHandler, NULL) == ESP_OK);
}

void sensorManagerDisableSpeedISR()
{
	if (g_readSensorDataTimerHandle != NULL) {
		esp_timer_stop(g_readSensorDataTimerHandle);
		esp_timer_delete(g_readSensorDataTimerHandle);
	}
	if (g_sendSensorDataTimerHandle != NULL) {
		esp_timer_stop(g_sendSensorDataTimerHandle);
		esp_timer_delete(g_sendSensorDataTimerHandle);
	}

	gpio_isr_handler_remove(GPIO_SPEED);
}

bool sensorManagerUpdateSpeed(void)
{
	// Calculate how much time between the two falling edges was
	const int64_t time = g_timeOfFallingEdgeSpeed - g_lastTimeOfFallingEdgeSpeed;

	// Convert the time to seconds
	const float fT = (float)time / 1000.0f;

	// Then save the speed frequency (rounded)
	g_speedInHz = (int)round(1000.0 / fT);

	// Is the speed value valid?
	if (g_speedInHz >= 500) g_speedInHz = 0;

	// Convert the frequency to actual speed
	const uint8_t oldSpeed = g_vehicleSpeed;
	g_vehicleSpeed = (int)calculateSpeedFromFrequency();

	if (oldSpeed != g_vehicleSpeed) {
		// Logging
		ESP_LOGD("SensorManager", "Speed changed! From: '%d kmh' to '%d kmh'", oldSpeed, g_vehicleSpeed);

		return true;
	}
	return false;
}

bool sensorManagerEnableRpmISR()
{
	return gpio_isr_handler_add(GPIO_RPM, rpmInterruptHandler, NULL) == ESP_OK;
}

void sensorManagerDisableRpmISR()
{
	gpio_isr_handler_remove(GPIO_RPM);
}

bool sensorManagerUpdateRPM(void)
{
	// Calculate how much time between the two falling edges was
	const int64_t time = g_timeOfFallingEdgeRPM - g_lastTimeOfFallingEdgeRPM;

	// Convert the time to seconds
	const float fT = (float)time / 1000.0f;

	// Then save the rpm frequency (rounded)
	g_rpmInHz = (int)round(1000.0 / fT);

	// Is the rpm value valid?
	if (g_rpmInHz >= 300) g_rpmInHz = -1;

	// Convert the frequency to actual rpm
	const int oldRpm = g_vehicleRPM;
	g_vehicleRPM = calculateRpmFromFrequency();

	if (oldRpm != g_vehicleRPM) {
		// Logging
		ESP_LOGD("SensorManager", "RPM changed! From: '%d RPM' to '%d RPM'", oldRpm, g_vehicleRPM);

		return true;
	}
	return false;
}

bool sensorManagerUpdateInternalTemperature(void)
{
	// Was the init successfully?
	if (g_initAdc2Failed) return false;

	// Try to get a reading from the ADC
	if (adc_oneshot_read(g_adc1Handle, ADC_CHANNEL_INT_TEMPERATURE, &g_intTempRawAdcValue) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Failed to read the internal temperature from the ADC!");
	}

	// Convert the adc raw value to a voltage (mV)
	adc_cali_raw_to_voltage(g_adc2IntTempCalibHandle, g_intTempRawAdcValue, &g_intTempVoltageMV);

	// Then calculate the temperature from the voltage
	double oldTemp = g_internalTemperature;
	g_internalTemperature = ((double)g_intTempVoltageMV - 540.0) / 10.0;

	if (oldTemp != g_internalTemperature) {
		return true;
	}
	return false;
}
