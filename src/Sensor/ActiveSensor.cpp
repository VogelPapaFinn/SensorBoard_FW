#include "Sensor/ActiveSensor.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "ActiveSensor";

/*
 *	Private Static Callback Functions
 */
static IRAM_ATTR void staticIsr(void* arg)
{
	if (arg == nullptr) {
		return;
	}

	ActiveSensor* instance = static_cast<ActiveSensor*>(arg);
	instance->isr();
}

/*
 *	Public Function Implementations
 */
ActiveSensor::ActiveSensor(const gpio_int_type_t& triggeringEdge)
{
	gpio_set_direction(gpio_, GPIO_MODE_INPUT);
	gpio_set_pull_mode(gpio_, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(gpio_, triggeringEdge);

	enable();
}

void ActiveSensor::enable()
{
	if (enabled_) {
		return;
	}

	if (gpio_isr_handler_add(gpio_, staticIsr, this) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to enable the ISR");
	}

	enabled_ = true;
}

void ActiveSensor::disable()
{
	if (!enabled_) {
		return;
	}

	if (gpio_isr_handler_remove(gpio_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to disable the ISR");
	}

	enabled_ = false;
}

int ActiveSensor::get() { return 0; }

void ActiveSensor::isr() {}
