#include "Sensor/Rpm.hpp"

// C++ includes
#include <math.h>

// espidf includes
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Rpm";

constexpr float MPH_TO_KMH = 1.60934;

constexpr uint16_t MAX_RPM = 8000;

/*
 *	Public Function Implementations
 */
Rpm::Rpm() : ActiveSensor(GPIO_INTR_POSEDGE) {}

int Rpm::get()
{
	int64_t time = 0;
	if (lastFallingEdgeTime_ != 0) {
		time = fallingEdgeTime_ - lastFallingEdgeTime_;
		lastFallingEdgeTime_ = 0; // Reset the time of the last falling edge, to detect if the rpm signal was lost
	}

	const float seconds = static_cast<float>(time) / 1000000.0f;

	hz_ = round(1.0 / seconds);

	calculateRpm();

	return rpm_;
}

void Rpm::isr()
{
	lastFallingEdgeTime_ = fallingEdgeTime_;
	fallingEdgeTime_ = esp_timer_get_time();
}

/*
 *	Private Functions Implementations
*/
void Rpm::calculateRpm()
{
	if (hz_ <= 0) {
		return;
	}

	double multiplier = 30.0;

	if (hz_ <= 8) {
		multiplier = 50.0;
	}
	else if (hz_ <= 11) {
		multiplier = 45.45;
	}
	else if (hz_ <= 17) {
		multiplier = 41.18;
	}
	else if (hz_ <= 25) {
		multiplier = 40.0;
	}
	else if (hz_ <= 56) {
		multiplier = 34.48;
	}
	else if (hz_ <= 92) {
		multiplier = 32.61;
	}
	else if (hz_ <= 123) {
		multiplier = 32.52;
	}
	else if (hz_ <= 157) {
		multiplier = 31.85;
	}
	else if (hz_ <= 188) {
		multiplier = 31.91;
	}
	else if (hz_ <= 220) {
		multiplier = 31.82;
	}
	else if (hz_ <= 262) {
		multiplier = 30.54;
	}

	// Calculate the rpm and return it
	rpm_ = static_cast<uint16_t>(hz_ * multiplier);
	if (rpm_ > MAX_RPM) {
		rpm_ = 0;
	}
}

