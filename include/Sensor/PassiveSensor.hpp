#pragma once

// espidf includes
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

class PassiveSensor
{
public:
	PassiveSensor(adc_oneshot_unit_handle_t* adc);

	void read();

	virtual int get();

protected:
	/*
	 *	Private Functions
	 */
	virtual void specificRead();

	static double calcVoltageDividerR2(int voltageMv, int r1);

	/*
	 *	Private Variables
	 */
	bool setup_ = false;

	int voltage_ = 0;

	adc_oneshot_unit_handle_t* adc_ = nullptr;

	adc_channel_t channel_ = ADC_CHANNEL_0;

	adc_oneshot_chan_cfg_t channelConfig_ = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};

	adc_cali_handle_t calibrationHandle_;

	gpio_num_t gpio_ = GPIO_NUM_NC;
};
