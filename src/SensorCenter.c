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
void sensorManagerInit()
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
	sensorsInitOilPressureSensor(&g_adc2Handle, &g_adcChannelConfig);
	sensorsInitFuelLevelSensor(&g_adc2Handle, &g_adcChannelConfig);
	sensorsInitWaterTemperatureSensor(&g_adc2Handle, &g_adcChannelConfig);
	sensorsInitInternalTemperatureSensor(&g_adc1Handle, &g_adcChannelConfig);
	sensorsInitIndicatorsSensor();

	// Init the automatic sensors
	sensorsInitSpeedSensor();
	sensorsInitRpmSensor();
}

void sensorsActivateISRs()
{
	sensorsActivateSpeedISR();
	sensorsActivateRpmISR();
}

void sensorsDeactivateISRs()
{
	sensorsDeactivateSpeedISR();
	sensorsDeactivateRpmISR();
}

void sensorsReadAll()
{
	// Read the fuel level
	sensorsReadFuelLevel();

	// Read the water temperature
	sensorsReadWaterTemperature();

	// Read the oil pressure
	sensorsReadOilPressure();

	// Read the internal temperature
	sensorsReadInternalTemperature();
}

void sensorsSendAll()
{
	// Create the CAN answer frame
	TwaiFrame_t frame;

	// Set the buffer content
	frame.buffer[0] = sensorsGetSpeed();
	const uint16_t rpm = sensorsGetRpm();
	frame.buffer[1] = rpm >> 8;
	frame.buffer[2] = (uint8_t)rpm;
	frame.buffer[3] = sensorsGetFuelLevel();
	frame.buffer[4] = sensorsGetWaterTemperature();
	frame.buffer[5] = sensorsIsOilPressurePresent();
	frame.buffer[6] = sensorsIsLeftIndicatorActive();
	frame.buffer[7] = sensorsIsRightIndicatorActive();

	// Initiate the frame
	canInitiateFrame(&frame, CAN_MSG_SENSOR_DATA, 8);

	// Send the frame
	canQueueFrame(&frame);
}
