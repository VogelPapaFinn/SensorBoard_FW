#include "Driver/Display.hpp"

// Project includes
#include "Global.h"
#include "Can.hpp"

/*
 *	Static Member Variables
 */
std::vector<uint8_t> Display::g_possibleCanIds = {1, 2, 3};

/*
 *	constexpr
 */
constexpr auto TAG = "Display";


/*
 *	Public function implementations
*/
Display::Display(const gpio_num_t powerGpio, const uint8_t& canId)
{
	powerGpio_ = powerGpio;
	if (powerGpio_ != GPIO_NUM_NC) {
		gpio_set_direction(powerGpio_, GPIO_MODE_OUTPUT);
	}

	canId_ = canId;
}

uint8_t Display::getCanId() const
{
	return canId_;
}

void Display::turnOn() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, GPIO_HIGH);
}

void Display::turnOff() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, GPIO_LOW);
}
