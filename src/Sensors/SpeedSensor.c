#include "Sensors/SpeedSensor.h"

// Project includes
#include "EventQueues.h"

// C includes
#include <math.h>

// espidf includes
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

/*
 *	Private defines
 */
#define SPEED_GPIO GPIO_NUM_3
#define MPH_TO_KMH 1.60934

/*
 *	Private variables
 */
static int64_t g_lastTimeOfFallingEdge = 0;
static int64_t g_timeOfFallingEdge = 0;

/*
 *	Private ISRs
 */
static void speedISR()
{
	g_lastTimeOfFallingEdge = g_timeOfFallingEdge;
	g_timeOfFallingEdge = esp_timer_get_time();
}

/*
 *	Private functions
 */
static uint8_t calculateSpeedFromFrequency(const double speedInHz)
{
	return (uint8_t)((speedInHz / 2.0) * MPH_TO_KMH); // Return in kmh
}

/*
 *	Public function implementations
 */
bool sensorsInitSpeedSensor()
{
	// Setup GPIO
	gpio_set_direction(SPEED_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(SPEED_GPIO, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(SPEED_GPIO, GPIO_INTR_ANYEDGE);

	// Activate the ISR
	if (gpio_isr_handler_add(SPEED_GPIO, speedISR, NULL) != ESP_OK) {
		ESP_LOGE("SpeedSensor", "Failed to enable the ISR for speed sensor");

		return false;
	}

	return true;
}

uint8_t sensorsGetSpeed()
{
	// Calculate how much time between the two falling edges was
	const int64_t time = g_timeOfFallingEdge - g_lastTimeOfFallingEdge;

	// Convert the time to seconds
	const double seconds = (float)time / 1000.0f;

	// Then save the speed frequency (rounded)
	const double speedInHz = (int)round(1000.0 / seconds);

	// Convert the frequency to actual speed
	return calculateSpeedFromFrequency(speedInHz);
}
