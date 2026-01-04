#include "Sensors/WaterTemperatureSensor.h"

// Project includes
#include "Sensors/SensorUtils.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_log.h"

/*
 *	Private defines
 */
#define WATER_TEMPERATURE_GPIO GPIO_NUM_11
#define WATER_TEMPERATURE_ADC_CHANNEL ADC_CHANNEL_0

#define WATER_TEMPERATURE_R1 3000

/*
 *	Private typedefs
 */
typedef struct
{
	uint8_t temp;
	uint16_t resistance;
} TempResistanceTuple_t;

/*
 *	Private Variables
 */
const TempResistanceTuple_t g_tempResistanceTuples[] = {
	{0, 5743},   {5, 4627},   {10, 3749},  {15, 3053},  {20, 2499},
	{25, 2056},  {30, 1700},  {35, 1412},  {40, 1178},  {45, 987},
	{50, 830},   {55, 701},   {60, 595},   {65, 507},   {70, 433},
	{75, 371},   {80, 319},   {85, 276},   {90, 239},   {95, 208},
	{100, 181},  {105, 158},  {110, 139},  {115, 122},  {120, 108}
};

const int g_amountOfTempResistanceTuples = sizeof(g_tempResistanceTuples) / sizeof(g_tempResistanceTuples[0]);

static adc_oneshot_unit_handle_t* g_adcHandle = NULL;

static adc_cali_handle_t g_adcCalibrationHandle;

static uint8_t g_waterTemperature = 0;

/*
 *	Private functions
 */
static uint8_t calculateTemperatureFromResistance(const uint16_t r) {
	// Check if it is below the first element
	if (r > g_tempResistanceTuples[0].resistance) {
		return g_tempResistanceTuples[0].temp;
	}

	// Check if it is above the last element
	if (r < g_tempResistanceTuples[g_amountOfTempResistanceTuples - 1].resistance) {
		return g_tempResistanceTuples[g_amountOfTempResistanceTuples - 1].temp + 1;
	}

	// Iterate through all entries
	for (int i = 0; i < g_amountOfTempResistanceTuples - 2; i++) {
		const uint16_t r1 = g_tempResistanceTuples[i].resistance;
		const uint16_t r2 = g_tempResistanceTuples[i + 1].resistance;

		// Check if the passed resistance is between this and the next entry
		if (r <= r1 && r >= r2) {
			const uint8_t temp1 = g_tempResistanceTuples[i].temp;
			const uint8_t temp2 = g_tempResistanceTuples[i].temp;

			const uint8_t t = (uint8_t)(temp1 + (r - r1) * ((temp2 - temp1) / (r2 - r1)));
			ESP_LOGI("WaterTemperatureSensor", "i: %d, r: %d, val: %d", i, r, t);

			// Calculate the value with linear interpolation
			// y = y1 + (x - x1) * (y2 - y1) / (x2 - x1)
			return (uint8_t)(temp1 + (r - r1) * ((temp2 - temp1) / (r2 - r1)));
		}
	}

	return 0;
}

/*
 *	Public function implementations
 */
bool sensorsInitWaterTemperatureSensor(const adc_oneshot_unit_handle_t* p_adcHandle,
                                       const adc_oneshot_chan_cfg_t* p_adcChannelConfig)
{
	// Save the adc handle
	g_adcHandle = (adc_oneshot_unit_handle_t*)p_adcHandle;

	// Set up the sensor
	if (sensorUtilsInitSensor(WATER_TEMPERATURE_GPIO, p_adcHandle, WATER_TEMPERATURE_ADC_CHANNEL, p_adcChannelConfig) != true) {
		ESP_LOGE("WaterTemperatureSensor", "Initialization failed");

		return false;
	}

	// Calibrate the sensor
	if (sensorUtilsCalibrateSensor(WATER_TEMPERATURE_ADC_CHANNEL, &g_adcCalibrationHandle) != true) {
		ESP_LOGE("WaterTemperatureSensor", "Calibration failed");

		return false;
	}

	return true;
}

void sensorsReadWaterTemperature()
{
	// Temporary containers
	int rawAdcValue = 0;
	int voltage = 0;

	// Try to read from the ADC
	if (adc_oneshot_read(*g_adcHandle,WATER_TEMPERATURE_ADC_CHANNEL, &rawAdcValue) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("WaterTemperatureSensor", "Failed to read from the ADC!");

		return;
	}

	// Try to convert the ADC value to a voltage
	if (adc_cali_raw_to_voltage(g_adcCalibrationHandle, rawAdcValue, &voltage) != ESP_OK) {
		// Log that it failed
		ESP_LOGW("WaterTemperatureSensor", "Failed to calculate the voltage from the ADC value!");

		return;
	}

	// Calculate resistance
	const double r2 = sensorUtilsCalculateVoltageDividerR2(voltage, WATER_TEMPERATURE_R1);

	// Calculate the water temperature from the calculated resistance
	g_waterTemperature = calculateTemperatureFromResistance((uint16_t)r2);
}

uint8_t sensorsGetWaterTemperature()
{
	return g_waterTemperature;
}
