#include "Sensor/FuelLevel.hpp"

#include <chrono>

/*
 *	constexpr
 */
constexpr auto TAG = "FuelLevel";

constexpr uint16_t R1 = 240;

constexpr double MAX_CHANGE_ALLOWER_P = 0.025; // 2.5%

/*
 *	Public Function Implementations
 */
FuelLevel::FuelLevel(adc_oneshot_unit_handle_t* adc) :
	PassiveSensor(GPIO_NUM_1, ADC_CHANNEL_0, adc)
{
}

int FuelLevel::get()
{
	return levelInPercent_;
}

/*
 *	Private Function Implementations
 */
void FuelLevel::specificRead()
{
	resistance_ = calcVoltageDividerR2(voltage_, R1);

	calcLevel();
}

void FuelLevel::calcLevel()
{
	lastResistance_ = resistance_;

	// R too low
	if (resistance_ < 3.0) {
		levelInPercent_ = 100;
	}

	// R too high
	if (resistance_ > 110.0) {
		levelInPercent_ = 0;
	}

	// Dont update the fuel level if it changed too much
	const double diff = 1 - (lastResistance_ / resistance_);
	if (diff > MAX_CHANGE_ALLOWER_P || diff < -MAX_CHANGE_ALLOWER_P) {
		return;
	}

	// Linear interpolation
	// y = y1 + (x - x1) * ((y2 - y1) / (x2 - x1))
	levelInPercent_ = 0.0 + (resistance_ - 110.0) * ((90.0 - 0.0) / (3.0 - 110.0));
	if (levelInPercent_ > 100.0) {
		levelInPercent_ = 100;
	}
}
