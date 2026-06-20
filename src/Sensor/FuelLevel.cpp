#include "Sensor/FuelLevel.hpp"

#include <chrono>

/*
 *	Private typedefs
 */
typedef struct
{
	uint8_t level; // %
	float r;
} LevelResistanceTuple_t;

/*
 *	constexpr
 */
constexpr auto TAG = "FuelLevel";

constexpr uint16_t R1 = 240;

constexpr double MAX_CHANGE_ALLOWER_P = 0.025; // 2.5%

constexpr LevelResistanceTuple_t levelResistanceTuples[] = {
	{100, 3},   {75, 15.8},   {50, 32.5},  {25, 64.2},  {0, 110}
};
constexpr uint8_t amountResistanceTuples = std::size(levelResistanceTuples);

/*
 *	Public Function Implementations
 */
FuelLevel::FuelLevel(adc_oneshot_unit_handle_t* adc) :
	PassiveSensor(GPIO_NUM_1, ADC_CHANNEL_0, adc)
{
}

int FuelLevel::get()
{
	// Calculate the level
	calcLevel();

	auto sortedLevels = lastLevels_;
	sortedLevels.sort();

	// Get the element in the middle
	auto it = sortedLevels.begin();
	std::advance(it, 50);

	return *it;
}

/*
 *	Private Function Implementations
 */
void FuelLevel::specificRead()
{
	resistance_ = calcVoltageDividerR2(voltage_, R1);
}

void FuelLevel::calcLevel()
{
	int levelInPercent = 0;

	// R too low
	if (resistance_ < levelResistanceTuples[0].r) {
		levelInPercent = 100;
	}

	// R too high
	if (resistance_ > levelResistanceTuples[amountResistanceTuples - 1].r) {
		levelInPercent = 0;
	}

	// Iterate through all entries
	for (int i = 0; i < amountResistanceTuples - 1; i++) {
		const uint16_t r1 = levelResistanceTuples[i].r;
		const uint16_t r2 = levelResistanceTuples[i + 1].r;

		// Check if the resistance is between this and the next entry
		if (resistance_ >= r1 && resistance_ <= r2) {
			const uint8_t level1 = levelResistanceTuples[i].level;
			const uint8_t level2 = levelResistanceTuples[i + 1].level;

			// Calculate the value with linear interpolation
			// y = y1 + (x - x1) * (y2 - y1) / (x2 - x1)
			levelInPercent = level1 + ((resistance_ - r1) * ((level2 - level1)) / (r2 - r1));

			break;
		}
	}

	// Track the value
	lastLevels_.pop_front();
	lastLevels_.push_back(levelInPercent);
}