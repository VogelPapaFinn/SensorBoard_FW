#include "Sensor/RightIndicator.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "RightIndicator";

/*
 *	Public Function Implementations
 */
RightIndicator::RightIndicator() : ActiveSensor(GPIO_INTR_ANYEDGE) {}

int RightIndicator::get()
{
	return high_;
}

void RightIndicator::isr()
{
	high_ = gpio_get_level(gpio_);
}

