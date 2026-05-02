#include "Sensor/FuelLevel.hpp"

/*
 *	constexpr
 */
constexpr auto TAG = "FuelLevel";

constexpr uint16_t R1 = 240;

/*
 *	Public Function Implementations
 */
FuelLevel::FuelLevel(adc_oneshot_unit_handle_t* adc) : PassiveSensor(adc) {}

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
	// R too low
	if (resistance_ < 3.0) {
		levelInPercent_ = 100;
	}

	// R too high
	if (resistance_ > 110.0) {
		levelInPercent_ = 0;
	}

	// TODO: Update & Test
	// Linear interpolation
	// y = y1 + (x - x1) * ((y2 - y1) / (x2 - x1))
	resistance_ = 0.0 + (resistance_ - 110.0) * ((100.0 - 0.0) / (3.0 - 110.0));
	if (resistance_ > 100.0) {
		levelInPercent_ = 100;
	}
}
