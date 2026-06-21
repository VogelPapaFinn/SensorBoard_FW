#include "Sensor/LeftIndicator.hpp"

// espidf includes
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	constexpr
 */
constexpr auto TAG = "LeftIndicator";

constexpr unsigned int ALLOW_CHANGE_AFTER_US = 50000;

/*
 *	Public Function Implementations
 */
LeftIndicator::LeftIndicator() : ActiveSensor(GPIO_NUM_15, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_FLOATING);
}

int LeftIndicator::get() { return active_; }

void LeftIndicator::cb() { active_ = gpio_get_level(gpio_) == 0; }
