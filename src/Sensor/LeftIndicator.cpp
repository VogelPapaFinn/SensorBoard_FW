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
LeftIndicator::LeftIndicator() :
	ActiveSensor(GPIO_NUM_15, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_PULLDOWN_ONLY);
}

int LeftIndicator::get()
{
	return high_;
}

void LeftIndicator::cb()
{
	high_ = gpio_get_level(gpio_);
}
