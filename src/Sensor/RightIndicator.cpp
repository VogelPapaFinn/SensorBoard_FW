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
	ActiveSensor(GPIO_NUM_15, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_PULLDOWN_ONLY);
}

int RightIndicator::get()
{
	return high_;
}

void RightIndicator::cb()
{
	high_ = gpio_get_level(gpio_);
}
