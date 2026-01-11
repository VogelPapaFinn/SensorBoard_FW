#include "SensorCenter.h"

// Project includes
#include "Sensors/FuelLevelSensor.h"
#include "Sensors/IndicatorsSensor.h"
#include "Sensors/InternalTemperatureSensor.h"
#include "Sensors/OilPressureSensor.h"
#include "Sensors/RpmSensor.h"
#include "Sensors/SpeedSensor.h"
#include "Sensors/WaterTemperatureSensor.h"
#include "can.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

/*
 *	Private variables
 */
//! \brief Boolean indicating if the initialization of the ADC2 was successful
static bool g_adc2Initialized = false;

//! \brief The handle of the ADC2
static adc_oneshot_unit_handle_t g_adc2Handle;

//! \brief The configuration which is used for all ADC1 and ADC2 channels
static adc_oneshot_chan_cfg_t g_adcChannelConfig = {
	.bitwidth = ADC_BITWIDTH_DEFAULT,
	.atten = ADC_ATTEN_DB_12,
};

//! \brief Boolean indicating if the initialization of the ADC1 was successful
static bool g_adc1Initialized = false;

//! \brief The handle of the ADC1
static adc_oneshot_unit_handle_t g_adc1Handle;

//! \brief Bool indicating if the ISR service was installed successfully. If not
//! speed, rpm and the indicators are not available
static bool g_isrServiceInstalled = false;

/*
 *	Public function implementations
*/
void sensorCenterInit()
{
	// Initialize the ADC1
	const adc_oneshot_unit_init_cfg_t adc1InitConfig = {
		.unit_id = ADC_UNIT_1,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc1InitConfig, &g_adc1Handle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Failed to initialize ADC1!");

		return;
	}

	// Initialization successful
	g_adc1Initialized = true;

	// Initialize the ADC2
	const adc_oneshot_unit_init_cfg_t adc2InitConfig = {
		.unit_id = ADC_UNIT_2,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	if (adc_oneshot_new_unit(&adc2InitConfig, &g_adc2Handle) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Failed to initialize ADC2!");

		return;
	}

	// Initialization successful
	g_adc2Initialized = true;

	// Install ISR service for
	g_isrServiceInstalled = true;
	if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) {
		// Logging
		ESP_LOGE("SensorManager", "Couldn't install the ISR service. Speed, RPM and Indicators are unavailable!");

		g_isrServiceInstalled = false;
	}

	// Init the manual sensors
	sensorOilPressureInit(&g_adc2Handle, &g_adcChannelConfig);
	sensorFuelLevelInit(&g_adc2Handle, &g_adcChannelConfig);
	sensorWaterTemperatureInit(&g_adc2Handle, &g_adcChannelConfig);
	sensorInternalTemperatureInit(&g_adc1Handle, &g_adcChannelConfig);
	sensorIndicatorsInit();

	// Init the automatic sensors
	sensorSpeedInit();
	sensorRpmInit();
}

void sensorsActivateISRs()
{
	sensorSpeedActivateISR();
	sensorRpmActivateISR();
}

void sensorsDeactivateISRs()
{
	sensorSpeedDeactivateISR();
	sensorRpmDeactivateISR();
}

void sensorsReadAll()
{
	// Read the fuel level
	sensorFuelLevelRead();

	// Read the water temperature
	sensorWaterTemperatureRead();

	// Read the oil pressure
	sensorOilPressureRead();

	// Read the internal temperature
	sensorInternalTemperatureRead();
}

void sensorsSendAll()
{
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the buffer content
	frame.buffer[0] = sensorSpeedGet();
	const uint16_t rpm = sensorRpmGet();
	frame.buffer[1] = rpm >> 8;
	frame.buffer[2] = (uint8_t)rpm;
	frame.buffer[3] = sensorFuelLevelGet();
	frame.buffer[4] = sensorWaterTemperatureGet();
	frame.buffer[5] = sensorOilPresserPresent();
	frame.buffer[6] = sensorIndicatorsLeftActive();
	frame.buffer[7] = sensorIndicatorsRightActive();

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_SENSOR_DATA, 8);

	// Send the frame
	canQueueFrame(&frame);
}
