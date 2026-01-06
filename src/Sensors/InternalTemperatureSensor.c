#include "Sensors/InternalTemperatureSensor.h"

// Project includes
#include "Sensors/SensorUtils.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_log.h"

/*
 *	Private defines
 */
#define INTERNAL_TEMPERATURE_GPIO GPIO_NUM_7
#define INTERNAL_TEMPERATURE_ADC_CHANNEL ADC_CHANNEL_6

/*
 *	Private Variables
 */
static adc_oneshot_unit_handle_t* g_adcHandle = NULL;

static adc_cali_handle_t g_adcCalibrationHandle;

static double g_internalTemperature = 0.0;

// #include "driver/temperature_sensor.h"
// static temperature_sensor_handle_t temp_handle = NULL;
// static temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
/*
 *	Public function implementations
 */
bool sensorsInitInternalTemperatureSensor(const adc_oneshot_unit_handle_t* p_adcHandle,
                                          const adc_oneshot_chan_cfg_t* p_adcChannelConfig)
{
	// Save the adc handle
	g_adcHandle = (adc_oneshot_unit_handle_t*)p_adcHandle;

	// Set up the sensor
	if (sensorUtilsInitSensor(INTERNAL_TEMPERATURE_GPIO, p_adcHandle, INTERNAL_TEMPERATURE_ADC_CHANNEL,
	                          p_adcChannelConfig) != true) {
		ESP_LOGE("InternalTemperatureSensor", "Initialization failed");

		return false;
	}

	// Calibrate the sensor
	if (sensorUtilsCalibrateSensor(INTERNAL_TEMPERATURE_ADC_CHANNEL, &g_adcCalibrationHandle) != true) {
		ESP_LOGE("InternalTemperatureSensor", "Calibration failed");

		return false;
	}

	// // Create the internal sensor
	// if (temperature_sensor_install(&temp_sensor_config, &temp_handle) != ESP_OK) {
	// 	ESP_LOGE("InternalTemperatureSensor", "Couldn't create the ESP32 internal temperature sensor");
	// 
	// 	return false;
	// }
	//
	// // Activate the internal sensor
	// if (temperature_sensor_enable(temp_handle) != ESP_OK) {
	// 	ESP_LOGE("InternalTemperatureSensor", "Couldn't activate the ESP32 internal temperature sensor");
	//
	// 	return false;
	// }

	return true;
}

void sensorsReadInternalTemperature()
{
	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	for (uint8_t i = 0; i < 64; i++) {
		int raw = 0;
		if (adc_oneshot_read(*g_adcHandle, INTERNAL_TEMPERATURE_ADC_CHANNEL, &raw) != ESP_OK) {
			// Log that it failed
			ESP_LOGW("InternalTemperatureSensor", "Failed to read from the ADC!");

			return;
		}
		rawAdcValue += raw;
	}
	rawAdcValue /= 64;

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adcCalibrationHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("InternalTemperatureSensor", "Failed to calculate the voltage from the ADC value!");

		return;
	}

	// Then calculate the temperature from the voltage
	g_internalTemperature = ((double)voltage - 500.0) / 10.0;

	// Get converted sensor data
	// loat tsens_out;
	// emperature_sensor_get_celsius(temp_handle, &tsens_out);
}

double sensorsGetInternalTemperature()
{
	return g_internalTemperature;
}
