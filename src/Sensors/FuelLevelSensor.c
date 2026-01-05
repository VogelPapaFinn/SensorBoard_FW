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

#define FUEL_LEVEL_FULL_R 3
#define FUEL_LEVEL_EMPTY_R 110

#define FUEL_LEVEL_R1 240

/*
 *	Private Variables
 */
static adc_oneshot_unit_handle_t* g_adcHandle = NULL;

static adc_cali_handle_t g_adcCalibrationHandle;

static uint8_t g_fuelLevel = false;

/*
 *	Private functions
 */
uint8_t calculateFuelLevelFromResistance(const double r)
{

	// Check if the resistance is too low
	if (r < 3.0) return 100;

	// Check if the resistance is too high
	if (r > 110.0) return 0;

	// Linear interpolation
	// y = y1 + (x - x1) * ((y2 - y1) / (x2 - x1))
	const double percent = 0.0 + (r - 110.0) * ((100.0 - 0.0) / (3.0 - 110.0));
	if (percent > 100.0) return 100;
	return (uint8_t)percent;
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
