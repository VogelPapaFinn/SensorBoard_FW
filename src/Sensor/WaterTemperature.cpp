#include "Sensor/WaterTemperature.hpp"

/*
 *	Private typedefs
 */
typedef struct
{
	uint8_t temp;
	uint16_t r;
} TempResistanceTuple_t;

/*
 *	constexpr
*/
constexpr auto TAG = "WaterTemperature";

constexpr uint16_t R1 = 3000;

constexpr TempResistanceTuple_t tempResistanceTuples[] = {
	{0, 5743},   {5, 4627},   {10, 3749},  {15, 3053},  {20, 2499},
	{25, 2056},  {30, 1700},  {35, 1412},  {40, 1178},  {45, 987},
	{50, 830},   {55, 701},   {60, 595},   {65, 507},   {70, 433},
	{75, 371},   {80, 319},   {85, 276},   {90, 239},   {95, 208},
	{100, 181},  {105, 158},  {110, 139},  {115, 122},  {120, 108}
};
constexpr uint8_t amountResistanceTuples = sizeof(tempResistanceTuples) / sizeof(tempResistanceTuples[0]);

/*
 *	Public Function Implementations
 */
WaterTemperature::WaterTemperature(adc_oneshot_unit_handle_t* adc) : PassiveSensor(adc) {}

int WaterTemperature::get()
{
	return temperature_;
}

/*
 *	Private Function Implementations
 */
void WaterTemperature::specificRead()
{
	resistance_ = calcVoltageDividerR2(voltage_, R1);
}

void WaterTemperature::calcTemperature(const uint16_t r)
{
	// Below our range
	if (r > tempResistanceTuples[0].r) {
		temperature_ = tempResistanceTuples[0].temp;
		return;
	}

	// Above our range
	if (r < tempResistanceTuples[amountResistanceTuples - 1].r) {
		temperature_ = tempResistanceTuples[amountResistanceTuples - 1].temp + 1;
		return;
	}
	// Iterate through all entries
	for (int i = 0; i < amountResistanceTuples - 2; i++) {
		const uint16_t r1 = tempResistanceTuples[i].r;
		const uint16_t r2 = tempResistanceTuples[i + 1].r;

		// Check if the passed resistance is between this and the next entry
		if (r <= r1 && r >= r2) {
			const uint8_t temp1 = tempResistanceTuples[i].temp;
			const uint8_t temp2 = tempResistanceTuples[i].temp;

			// Calculate the value with linear interpolation
			// y = y1 + (x - x1) * (y2 - y1) / (x2 - x1)
			temperature_ = temp1 + (r - r1) * ((temp2 - temp1) / (r2 - r1));
			return;
		}
	}
}
