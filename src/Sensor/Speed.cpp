#include "Sensor/Speed.hpp"

// C++ includes
#include <math.h>

// espidf includes
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Speed";

constexpr float MPH_TO_KMH = 1.60934;

/*
 *	Public Function Implementations
 */
Speed::Speed() : ActiveSensor(GPIO_INTR_POSEDGE) {}

int Speed::get()
{
	int64_t time = 0;
	if (lastFallingEdgeTime_ != 0) {
		time = fallingEdgeTime_ - lastFallingEdgeTime_;
		lastFallingEdgeTime_ = 0; // Reset the time of the last falling edge, to detect if the rpm signal was lost
	}

	const float seconds = static_cast<float>(time) / 1000000.0f;

	hz_ = round(1.0 / seconds);

	calculateKmh();

	return kmh_;
}

void Speed::isr()
{
	lastFallingEdgeTime_ = fallingEdgeTime_;
	fallingEdgeTime_ = esp_timer_get_time();
}

/*
 *	Private Functions Implementations
 */
void Speed::calculateKmh()
{
	kmh_ = static_cast<uint8_t>((hz_ / 2.0) * MPH_TO_KMH); // kmh
}
