#include "Sensors/OilPressureSensor.h"

// Project includes
#include "Sensors/SensorUtils.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_log.h"

/*
 *	Private defines
 */
#define OIL_PRESSURE_GPIO GPIO_NUM_12
#define OIL_PRESSURE_ADC_CHANNEL ADC_CHANNEL_1

#define OIL_PRESSURE_LOWER_THRESHOLD_MV 65 // mV -> R2 ~= 5 Ohms
#define OIL_PRESSURE_UPPER_THRESHOLD_MV 255// mV -> R2 ~= 20 Ohms

/*
 *	Private Variables
 */
static adc_oneshot_unit_handle_t* g_adcHandle = NULL;

static adc_cali_handle_t g_adcCalibrationHandle;

static bool g_oilPressurePresent = false;

/*
 *	Public function implementations
 */
bool sensorsInitOilPressureSensor(const adc_oneshot_unit_handle_t* p_adcHandle,
                                  const adc_oneshot_chan_cfg_t* p_adcChannelConfig)
{
	// Save the adc handle
	g_adcHandle = (adc_oneshot_unit_handle_t*)p_adcHandle;

	// Set up the sensor
	if (sensorUtilsInitSensor(OIL_PRESSURE_GPIO, p_adcHandle, OIL_PRESSURE_ADC_CHANNEL, p_adcChannelConfig) != true) {
		ESP_LOGE("OilPressureSensor", "Initialization failed");

		return false;
	}

	// Calibrate the sensor
	if (sensorUtilsCalibrateSensor(OIL_PRESSURE_ADC_CHANNEL, &g_adcCalibrationHandle) != true) {
		ESP_LOGE("OilPressureSensor", "Calibration failed");

		return false;
	}

	return true;
}

void sensorsReadOilPressure()
{
	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(*g_adcHandle, OIL_PRESSURE_ADC_CHANNEL, &rawAdcValue) != ESP_OK) {
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

	// Check the thresholds
	g_oilPressurePresent = voltage > OIL_PRESSURE_LOWER_THRESHOLD_MV && voltage < OIL_PRESSURE_UPPER_THRESHOLD_MV;
}

bool sensorsIsOilPressurePresent()
{
	return g_oilPressurePresent;
}
