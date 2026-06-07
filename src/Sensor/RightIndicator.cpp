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
RightIndicator::RightIndicator() :
	ActiveSensor(GPIO_NUM_7, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_FLOATING);
}

int RightIndicator::get()
{
	return active_;
}

void RightIndicator::cb()
{
	active_ = !static_cast<bool>(gpio_get_level(gpio_));
}
