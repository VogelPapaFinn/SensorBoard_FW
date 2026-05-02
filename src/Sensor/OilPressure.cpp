#include "Sensor/OilPressure.hpp"

/*
 *	constexpr
 */
constexpr uint8_t LOWER_THRESHOLD_MV = 65;	// mV -> R2 ~= 5 Ohms
constexpr uint8_t UPPER_THRESHOLD_MV = 255; // mV -> R2 ~= 20 Ohms

/*
 *	Public Function Implementations
 */
OilPressure::OilPressure(adc_oneshot_unit_handle_t* adc) : PassiveSensor(adc) {}

int OilPressure::get()
{
	return pressure_;
}

void OilPressure::specificRead()
{
	pressure_ = voltage_ > LOWER_THRESHOLD_MV && voltage_ < UPPER_THRESHOLD_MV;
}
