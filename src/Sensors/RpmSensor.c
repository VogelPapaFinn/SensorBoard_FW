#include "Sensors/RpmSensor.h"

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
#define RPM_GPIO GPIO_NUM_21
#define MAX_RPM 8000

/*
 *	Private variables
 */
static int64_t g_lastTimeOfFallingEdge = 0;
static int64_t g_timeOfFallingEdge = 0;

/*
 *	Private ISRs
 */
static void rpmISR()
{
	g_lastTimeOfFallingEdge = g_timeOfFallingEdge;
	g_timeOfFallingEdge = esp_timer_get_time();
}

/*
 *	Private functions
 */
static uint16_t calculateRpm(const double rpmInHz)
{
	double multiplier = 30.0;

	// Get the multiplier
	if (rpmInHz <= 0)
		return 0;
	if (rpmInHz <= 8)
		multiplier = 50.0;
	else if (rpmInHz <= 11)
		multiplier = 45.45;
	else if (rpmInHz <= 17)
		multiplier = 41.18;
	else if (rpmInHz <= 25)
		multiplier = 40.0;
	else if (rpmInHz <= 56)
		multiplier = 34.48;
	else if (rpmInHz <= 92)
		multiplier = 32.61;
	else if (rpmInHz <= 123)
		multiplier = 32.52;
	else if (rpmInHz <= 157)
		multiplier = 31.85;
	else if (rpmInHz <= 188)
		multiplier = 31.91;
	else if (rpmInHz <= 220)
		multiplier = 31.82;
	else if (rpmInHz <= 262)
		multiplier = 30.54;

	// Calculate the rpm and return it
	return (uint16_t)(rpmInHz * multiplier);
}

/*
 *	Public function implementations
 */
bool sensorRpmInit()
{
	// Setup GPIO
	gpio_set_direction(RPM_GPIO, GPIO_MODE_INPUT);
	gpio_set_pull_mode(RPM_GPIO, GPIO_PULLDOWN_ONLY);
	gpio_set_intr_type(RPM_GPIO, GPIO_INTR_POSEDGE);

	// Activate the ISR
	if (gpio_isr_handler_add(RPM_GPIO, rpmISR, NULL) != ESP_OK) {
		ESP_LOGE("RpmSensor", "Failed to enable the ISR for rpm sensor");

		return false;
	}

	return true;
}

void sensorRpmActivateISR()
{
	// Activate the ISR
	if (gpio_isr_handler_add(RPM_GPIO, rpmISR, NULL) != ESP_OK) {
		ESP_LOGE("RPMSensor", "Failed to enable the ISR for rpm sensor");
	}
}

void sensorRpmDeactivateISR()
{
	// Deactivate the ISR
	if (gpio_isr_handler_remove(RPM_GPIO) != ESP_OK) {
		ESP_LOGE("RPMSensor", "Failed to disable the ISR for rpm sensor");
	}
}

uint16_t sensorRpmGet()
{
	// Calculate how much time between the two falling edges was
	int64_t time = 0;
	if (g_lastTimeOfFallingEdge != 0) {
		time = g_timeOfFallingEdge - g_lastTimeOfFallingEdge;
		g_lastTimeOfFallingEdge = 0; // Reset the time of the last falling edge, to detect if the rpm signal was lost
	}

	// Convert the time to seconds
	const float seconds = (float)time / 1000000.0f;

	// Then save the rpm frequency (rounded)
	const double rpmInHz = round(1.0 / seconds);

	// Calculate the RPM
	uint16_t rpm = calculateRpm(rpmInHz);
	if (rpm >= MAX_RPM) {
		rpm = 0;
	}

	return rpm;
}
