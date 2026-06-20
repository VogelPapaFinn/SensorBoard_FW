#include "Sensor/FuelLevel.hpp"

// C++ includes
#include <cmath>

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
constexpr float NEW_VALUES_DAMPENER = 0.01f;
constexpr LevelResistanceTuple_t LEVEL_RESISTANCE_TUPLES[] = {
	{100, 3},   {75, 15.8},   {50, 32.5},  {25, 64.2},  {0, 110}
};
constexpr uint8_t AMOUNT_LEVEL_TUPLES = std::size(LEVEL_RESISTANCE_TUPLES);

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

	/*
	 *	Median Filter
	 */
	auto sortedLevels = lastLevels_;
	sortedLevels.sort();

	// Get the element in the middle
	auto it = sortedLevels.begin();
	std::advance(it, 10);
	const float median = *it;

	/*
	 *	Dampening new values
	 */
	// Initial value
	if (smoothedValue_ < 0) {
		smoothedValue_ = median;
	}

	// Calculate the dampened value
	smoothedValue_ = (NEW_VALUES_DAMPENER * median) + ((1.0f - NEW_VALUES_DAMPENER) * smoothedValue_);

	return std::round(static_cast<int>(smoothedValue_));
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
	if (resistance_ < LEVEL_RESISTANCE_TUPLES[0].r) {
		levelInPercent = 100;
	}

	// R too high
	if (resistance_ > LEVEL_RESISTANCE_TUPLES[AMOUNT_LEVEL_TUPLES - 1].r) {
		levelInPercent = 0;
	}

	// Iterate through all entries
	for (int i = 0; i < AMOUNT_LEVEL_TUPLES - 1; i++) {
		const uint16_t r1 = LEVEL_RESISTANCE_TUPLES[i].r;
		const uint16_t r2 = LEVEL_RESISTANCE_TUPLES[i + 1].r;

		// Check if the resistance is between this and the next entry
		if (resistance_ >= r1 && resistance_ <= r2) {
			const uint8_t level1 = LEVEL_RESISTANCE_TUPLES[i].level;
			const uint8_t level2 = LEVEL_RESISTANCE_TUPLES[i + 1].level;

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