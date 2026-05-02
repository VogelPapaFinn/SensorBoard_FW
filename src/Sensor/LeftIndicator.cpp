#include "Sensor/LeftIndicator.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "LeftIndicator";

/*
 *	Public Function Implementations
 */
LeftIndicator::LeftIndicator() : ActiveSensor(GPIO_INTR_ANYEDGE) {}

int LeftIndicator::get()
{
	return high_;
}

void LeftIndicator::isr()
{
	high_ = gpio_get_level(gpio_);
}

