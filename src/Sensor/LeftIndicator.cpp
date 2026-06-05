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
	gpio_set_pull_mode(gpio_, GPIO_FLOATING);
}

int LeftIndicator::get()
{
	return active_;
}

void LeftIndicator::cb()
{
	active_ = !static_cast<bool>(gpio_get_level(gpio_));
	esp_rom_printf("LeftIndicator: %d \n", active_);
}
