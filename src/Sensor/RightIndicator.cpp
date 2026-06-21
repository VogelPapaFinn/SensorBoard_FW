#include "Sensor/RightIndicator.hpp"

// espidf includes
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	constexpr
 */
constexpr auto TAG = "RightIndicator";

constexpr unsigned int ALLOW_CHANGE_AFTER_US = 50000;

/*
 *	Public Function Implementations
 */
RightIndicator::RightIndicator() : ActiveSensor(GPIO_NUM_7, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_FLOATING);
}

int RightIndicator::get() { return active_; }

void RightIndicator::cb() { active_ = gpio_get_level(gpio_) == 0; }
