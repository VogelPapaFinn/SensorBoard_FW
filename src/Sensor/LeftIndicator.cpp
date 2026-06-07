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
LeftIndicator::LeftIndicator() : ActiveSensor(GPIO_NUM_15, GPIO_INTR_ANYEDGE)
{
	gpio_set_pull_mode(gpio_, GPIO_FLOATING);
}

int LeftIndicator::get() { return active_; }

#include "esp_timer.h"
void LeftIndicator::cb()
{
	uint64_t current_time = esp_timer_get_time();

	// Zustand nur aktualisieren, wenn die Sperrzeit abgelaufen ist
	if ((current_time - lastIsrTime_) > 10000) {
		active_ = !static_cast<bool>(gpio_get_level(gpio_));
		lastIsrTime_ = current_time;
	}
}
