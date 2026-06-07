#include "Sensor/PassiveSensor.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "PassiveSensor";
constexpr double VOLTAGE = 3.3;
constexpr unsigned int ADC_SAMPLE_COUNT = 10;

/*
 *	Public Function Implementations
 */
PassiveSensor::PassiveSensor(gpio_num_t gpio, adc_channel_t adcChannel, adc_oneshot_unit_handle_t* adc, adc_unit_t unit)
{
	gpio_ = gpio;
	channel_ = adcChannel;
	adc_ = adc;
	unit_ = unit;

	// Configure the channel
	if (adc_oneshot_config_channel(*adc_, channel_, &channelConfig_) != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't set adc config for channel %d", channel_);
		return;
	}

	// Calibrate
	const adc_cali_curve_fitting_config_t calibrationConfig = {
		.unit_id = unit_,
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
	unsigned int successfullReads = 0;
	uint32_t adcValueSum = 0;
	for (unsigned int i = 0; i < ADC_SAMPLE_COUNT; i++) {
		if (adc_oneshot_read(*adc_, channel_, &adcValue) == ESP_OK) {
			successfullReads++;
			adcValueSum += adcValue;
		} else {
			ESP_LOGW(TAG, "Failed to read channel %d", channel_);
		}
	}

	if (successfullReads == 0) {
		ESP_LOGW(TAG, "Failed to read any data from channel %d", channel_);
		return;
	}

	if (adc_cali_raw_to_voltage(calibrationHandle_, adcValueSum / successfullReads, &voltage_) != ESP_OK) {
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
	const double vOut = static_cast<double>(voltageMv) / 1000.0;
	const double r = static_cast<double>(r1);

	// Prevents divison by 0
	if (vOut >= VOLTAGE) {
		return 0.0;
	}

	// R2 = R1 * (voltageMv / (preR1VoltageV - voltageMv))
	return r * (vOut / (VOLTAGE - vOut));
}