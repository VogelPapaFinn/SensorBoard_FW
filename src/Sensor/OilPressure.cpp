#include "Sensor/OilPressure.hpp"

/*
 *	constexpr
 */
constexpr uint8_t ENGINE_OFF_MV = 200;	// no oil pressure -> 0 ohms
constexpr uint8_t ENGINE_ON_MV = 2800;  // with oil pressure -> 5.1k ohms

/*
 *	Public Function Implementations
 */
OilPressure::OilPressure(adc_oneshot_unit_handle_t* adc) : PassiveSensor(GPIO_NUM_2, ADC_CHANNEL_1, adc, ADC_UNIT_1) {}

int OilPressure::get()
{
	return pressure_;
}

#include "esp_log.h"
void OilPressure::specificRead()
{
	pressure_ = voltage_ > ENGINE_OFF_MV && voltage_ >= ENGINE_ON_MV;
}
