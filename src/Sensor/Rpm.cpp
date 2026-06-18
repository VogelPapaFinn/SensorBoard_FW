#include "Sensor/Rpm.hpp"

// C++ includes
#include <math.h>

// espidf includes
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	Private typedefs
 */
typedef struct
{
	uint16_t freq;
	double multiplier;
} FrequencyMultiplierTuple_t;

/*
 *	constexpr
 */
constexpr auto TAG = "Rpm";

constexpr float MPH_TO_KMH = 1.60934;

constexpr uint16_t MAX_RPM = 8000;

constexpr FrequencyMultiplierTuple_t FREQ_MULTIPLIER_TUPLES[] = {
	{8, 50},	  {11, 45.45},	{17, 41.18},  {25, 40.0},	{56, 34.48},  {92, 32.61},
	{123, 32.52}, {157, 31.85}, {188, 31.91}, {220, 31.82}, {262, 30.54},
};
constexpr uint8_t AMOUNT_FREQ_MULTIPLIER_TUPLES = sizeof(FREQ_MULTIPLIER_TUPLES) / sizeof(FREQ_MULTIPLIER_TUPLES[0]);

/*
 *	Public Function Implementations
 */
Rpm::Rpm() : ActiveSensor(GPIO_NUM_9, GPIO_INTR_NEGEDGE) {}

int Rpm::get()
{
	portENTER_CRITICAL_ISR(&mux_);
	const int64_t time = fallingEdgeTime_ - lastFallingEdgeTime_;
	portEXIT_CRITICAL_ISR(&mux_);

	const float seconds = static_cast<float>(time) / 1000000.0f;
	hz_ = round(1.0 / seconds);
	calculateRpm();

	return rpm_;
}

void Rpm::cb()
{
	portENTER_CRITICAL_ISR(&mux_);
	lastFallingEdgeTime_ = fallingEdgeTime_;
	fallingEdgeTime_ = esp_timer_get_time();
	portEXIT_CRITICAL_ISR(&mux_);
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

	// Frequency is lower than the first data point
	if (hz_ <= FREQ_MULTIPLIER_TUPLES[0].freq) {
		multiplier = FREQ_MULTIPLIER_TUPLES[0].multiplier;
	}

	// Its Frequency is higher than the last data point
	else if (hz_ >= FREQ_MULTIPLIER_TUPLES[AMOUNT_FREQ_MULTIPLIER_TUPLES - 1].freq) {
		multiplier = FREQ_MULTIPLIER_TUPLES[AMOUNT_FREQ_MULTIPLIER_TUPLES - 1].multiplier;
	}

	// Interpolate the multiplier
	else {
		for (uint8_t i = 0; i < AMOUNT_FREQ_MULTIPLIER_TUPLES; i++) {
			if (hz_ >= FREQ_MULTIPLIER_TUPLES[i].freq && hz_ <= FREQ_MULTIPLIER_TUPLES[i + 1].freq) {
				const double x0 = FREQ_MULTIPLIER_TUPLES[i].freq;
				const double y0 = FREQ_MULTIPLIER_TUPLES[i].multiplier;
				const double x1 = FREQ_MULTIPLIER_TUPLES[i + 1].freq;
				const double y1 = FREQ_MULTIPLIER_TUPLES[i + 1].multiplier;

				// Linear interpolation see https://en.wikipedia.org/wiki/Linear_interpolation
				multiplier = y0 + (hz_ - x0) * ((y1 - y0) / (x1 - x0));
				break;
			}
		}
	}

	// Calculate the rpm and return it
	rpm_ = static_cast<uint16_t>(hz_ * multiplier);
	if (rpm_ > MAX_RPM) {
		rpm_ = 0;
	}
}
