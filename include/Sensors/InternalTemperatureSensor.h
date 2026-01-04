#pragma once

// espidf includes
#include "esp_adc/adc_oneshot.h"

bool sensorsInitInternalTemperatureSensor(const adc_oneshot_unit_handle_t* p_adcHandle, const adc_oneshot_chan_cfg_t* p_adcChannelConfig);

void sensorsReadInternalTemperature();

double sensorsGetInternalTemperature();