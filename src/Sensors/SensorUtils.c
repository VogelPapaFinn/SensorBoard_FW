#include "Sensors/SensorUtils.h"

// espidf includes
#include "esp_log.h"

/*
 *	Private defines
 */
#define VOLTAGE_LEVEL 3.3

/*
 *	Private variables
 */

/*
 *	Public function implementations
 */
bool sensorUtilsInitSensor(const gpio_num_t gpio, const adc_oneshot_unit_handle_t* p_adcHandle, const adc_channel_t adcChannel, const adc_oneshot_chan_cfg_t* p_adcChannelConfig)
{
	// Set up the GPIO
	gpio_set_direction(gpio, GPIO_MODE_INPUT);
	gpio_set_pull_mode(gpio, GPIO_PULLDOWN_ONLY);

	// Configure the channel
	if (adc_oneshot_config_channel(*p_adcHandle, adcChannel, p_adcChannelConfig) != ESP_OK) {
		return false;
	}
	return true;
}

bool sensorUtilsCalibrateSensor(const adc_channel_t adcChannel, adc_cali_handle_t* p_adcCalibrationHandle)
{
	// Create config
	const adc_cali_curve_fitting_config_t curveCalibrationConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = adcChannel,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};

	// Apply curve calibration
	if (adc_cali_create_scheme_curve_fitting(&curveCalibrationConfig, p_adcCalibrationHandle) != ESP_OK) {
		return false;
	}

	return true;
}

double sensorUtilsCalculateVoltageDividerR2(const int voltageMV, const int r1)
{
	const float vOut = (float)voltageMV / 1000.0f;
	const float r = (float)r1;

	// R2 = R1 * (voltageMV / (preR1VoltageV - voltageMV))
	return (float)r * (vOut / (VOLTAGE_LEVEL - vOut));
}