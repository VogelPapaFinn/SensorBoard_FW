#include "Sensors/FuelLevelSensor.h"

// Project includes
#include "Sensors/SensorUtils.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_log.h"

/*
 *	Private defines
 */
#define FUEL_LEVEL_GPIO GPIO_NUM_13
#define FUEL_LEVEL_ADC_CHANNEL ADC_CHANNEL_2

#define FUEL_LEVEL_R1 240
#define FUEL_LEVEL_OFFSET 5.0f
#define FUEL_LEVEL_TO_PERCENTAGE 115.0f// Divide the calculated resistance by this value to get the level in percent

/*
 *	Private Variables
 */
static adc_oneshot_unit_handle_t* g_adcHandle = NULL;

static adc_cali_handle_t g_adcCalibrationHandle;

static uint8_t g_fuelLevel = false;

/*
 *	Private functions
 */
int calculateFuelLevelFromResistance(const double r)
{
	float resistance = r;

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

/*
 *	Public function implementations
 */
bool sensorsInitFuelLevelSensor(const adc_oneshot_unit_handle_t* p_adcHandle,
                                const adc_oneshot_chan_cfg_t* p_adcChannelConfig)
{
	// Save the adc handle
	g_adcHandle = (adc_oneshot_unit_handle_t*)p_adcHandle;

	// Set up the sensor
	if (sensorUtilsInitSensor(FUEL_LEVEL_GPIO, p_adcHandle, FUEL_LEVEL_ADC_CHANNEL, p_adcChannelConfig) != true) {
		ESP_LOGE("FuelLevelSensor", "Initialization failed");

		return false;
	}

	// Calibrate the sensor
	if (sensorUtilsCalibrateSensor(FUEL_LEVEL_ADC_CHANNEL, &g_adcCalibrationHandle) != true) {
		ESP_LOGE("FuelLevelSensor", "Calibration failed");

		return false;
	}

	return true;
}

void sensorsReadFuelLevel()
{
	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(*g_adcHandle, FUEL_LEVEL_ADC_CHANNEL, &rawAdcValue) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("OilPressureSensor", "Failed to read from the ADC!");

		return;
	}

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adcCalibrationHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("OilPressureSensor", "Failed to calculate the voltage from the ADC value!");

		return;
	}

	// Calculate resistance
	const double resistance = sensorUtilsCalculateVoltageDividerR2(voltage, FUEL_LEVEL_R1);

	// Calculate the fuel level from the calculated resistance
	g_fuelLevel = calculateFuelLevelFromResistance(resistance);
}

uint8_t sensorsGetFuelLevel()
{
	return g_fuelLevel;
}
