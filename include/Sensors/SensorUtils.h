#pragma once

// espidf includes
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

/*
 *	Public functions
 */
bool sensorUtilsInitSensor(gpio_num_t gpio, const adc_oneshot_unit_handle_t* p_adcHandle, adc_channel_t adcChannel, const adc_oneshot_chan_cfg_t* p_adcChannelConfig);

bool sensorUtilsCalibrateSensor(adc_channel_t adcChannel, adc_cali_handle_t* p_adcCalibrationHandle);

double sensorUtilsCalculateVoltageDividerR2(int voltageMV, int r1);