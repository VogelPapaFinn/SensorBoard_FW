#include "Sensors/IndicatorsSensor.h"

// espidf includes
#include "driver/gpio.h"

/*
 *	Private defines
 */
#define LEFT_INDICATOR_GPIO GPIO_NUM_10
#define RIGHT_INDICATOR_GPIO GPIO_NUM_9

/*
 *	Public function implementations
 */
bool sensorIndicatorsInit()
{
	// Setup GPIO for the left indicator
	gpio_set_direction(LEFT_INDICATOR_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(LEFT_INDICATOR_GPIO, GPIO_PULLDOWN_ONLY);

	// Setup GPIO for the right indicator
	gpio_set_direction(RIGHT_INDICATOR_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(RIGHT_INDICATOR_GPIO, GPIO_PULLDOWN_ONLY);

	return true;
}

bool sensorIndicatorsLeftActive()
{
	return gpio_get_level(LEFT_INDICATOR_GPIO);
}

bool sensorIndicatorsRightActive()
{
	return gpio_get_level(RIGHT_INDICATOR_GPIO);
}
