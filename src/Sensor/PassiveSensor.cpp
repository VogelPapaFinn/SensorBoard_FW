#include "Sensor/PassiveSensor.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "PassiveSensor";
constexpr double VOLTAGE = 3.3;

/*
 *	Public Function Implementations
 */
PassiveSensor::PassiveSensor(adc_oneshot_unit_handle_t* adc)
{
	adc_ = adc;

	gpio_set_direction(gpio_, GPIO_MODE_INPUT);
	gpio_set_pull_mode(gpio_, GPIO_PULLDOWN_ONLY);

	// Configure the channel
	if (adc_oneshot_config_channel(*adc_, channel_, &channelConfig_) != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't set adc config for channel %d", channel_);
		return;
	}

	// Calibrate
	const adc_cali_curve_fitting_config_t calibrationConfig = {
		.unit_id = ADC_UNIT_2,
		.chan = channel_,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	if (adc_cali_create_scheme_curve_fitting(&calibrationConfig, &calibrationHandle_) != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't calibrate adc channel %d", channel_);
		return;
	}

	setup_ = true;
}

void PassiveSensor::read()
{
	if (!setup_) {
		return;
	}

	int adcValue = 0;

	if (adc_oneshot_read(*adc_, channel_, &adcValue) != ESP_OK) {
		ESP_LOGW(TAG, "Failed to read channel %d", channel_);
		return;
	}

	if (adc_cali_raw_to_voltage(calibrationHandle_, adcValue, &voltage_) != ESP_OK) {
		voltage_ = 0;
		ESP_LOGW(TAG, "Couldn't convert adc channel %d value to voltage", channel_);
		return;
	}

	specificRead();
}

int PassiveSensor::get()
{
	return voltage_;
}

void PassiveSensor::specificRead() {}

double PassiveSensor::calcVoltageDividerR2(const int voltageMv, const int r1)
{
	const float vOut = static_cast<float>(voltageMv) / 1000.0f;
	const float r = static_cast<float>(r1);

	// R2 = R1 * (voltageMv / (preR1VoltageV - voltageMv))
	return static_cast<float>(r) * (vOut / (VOLTAGE - vOut));
}