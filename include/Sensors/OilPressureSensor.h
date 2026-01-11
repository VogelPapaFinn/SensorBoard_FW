#pragma once

// espidf includes
#include "esp_adc/adc_oneshot.h"

bool sensorOilPressureInit(const adc_oneshot_unit_handle_t* p_adcHandle, const adc_oneshot_chan_cfg_t* p_adcChannelConfig);

void sensorOilPressureRead();

bool sensorOilPresserPresent();