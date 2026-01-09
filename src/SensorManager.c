#include "SensorManager.h"

// Project includes
#include "can.h"
#include "Sensors/FuelLevelSensor.h"
#include "Sensors/IndicatorsSensor.h"
#include "Sensors/InternalTemperatureSensor.h"
#include "Sensors/OilPressureSensor.h"
#include "Sensors/RpmSensor.h"
#include "Sensors/SpeedSensor.h"
#include "Sensors/WaterTemperatureSensor.h"

// espidf includes
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

/*
 *	Private defines
 */
#define SEND_SENSOR_DATA_INTERVAL_MS 50

/*
 *	Prototypes
 */
//! \brief ISR used to send all available sensor data periodically via CAN
//! \param p_arg Unused pointer needed for espidf so this can be used as timer ISR
static void sendSensorDataISR(void* p_arg);

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

//! \brief Handle of timer which is used to periodically send all available sensor data via CAN
static esp_timer_handle_t g_sendSensorDataTimerHandle;

//! \brief The configuration of the timer which is used to periodically send all available sensor data via CAN
static const esp_timer_create_args_t g_sendSensorDataTimerConf = {.callback = &sendSensorDataISR,
                                                                  .name = "Send Sensor Data Timer"};


/*
 *	Private ISRs and Tasks
 */
static void sendSensorDataISR(void* p_arg)
{
	// Update all sensors first (not all of them need the manual update)
	sensorsReadFuelLevel();
	sensorsReadWaterTemperature();
	sensorsReadOilPressure();
	sensorsReadInternalTemperature();

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
	canInitiateFrame(&frame, CAN_MSG_SENSOR_DATA, g_ownCanComId, 8);

	// Send the frame
	// canQueueFrame(&frame);
}

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
}

bool sensorsStartReadingAllSensors()
{
	bool success = true;

	// Start the manual sensors
	success &= sensorsInitOilPressureSensor(&g_adc2Handle, &g_adcChannelConfig);
	success &= sensorsInitFuelLevelSensor(&g_adc2Handle, &g_adcChannelConfig);
	success &= sensorsInitWaterTemperatureSensor(&g_adc2Handle, &g_adcChannelConfig);
	success &= sensorsInitInternalTemperatureSensor(&g_adc1Handle, &g_adcChannelConfig);
	success &= sensorsInitIndicatorsSensor();

	// Start the automatic sensors
	success &= sensorsInitSpeedSensor();
	success &= sensorsInitRpmSensor();

	return success;
}

bool sensorsStartSendingSensorData()
{
	// Create the timer
	if (g_sendSensorDataTimerHandle == NULL) {
		esp_timer_create(&g_sendSensorDataTimerConf, &g_sendSensorDataTimerHandle);
	}

	// Start the timer
	if (!esp_timer_is_active(g_sendSensorDataTimerHandle)) {
		return esp_timer_start_periodic(g_sendSensorDataTimerHandle, (uint64_t)(SEND_SENSOR_DATA_INTERVAL_MS * 1000)) == ESP_OK; // NOLINT
	}

	return false;
}

void sensorsStopSendingSensorData()
{
	if (g_sendSensorDataTimerHandle == NULL) {
		esp_timer_stop(g_sendSensorDataTimerHandle);
		esp_timer_delete(g_sendSensorDataTimerHandle);
		g_sendSensorDataTimerHandle = NULL;
	}
}
