#include "Core.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "Core";

constexpr gpio_num_t GPIO_CAN_RX = GPIO_NUM_41;
constexpr gpio_num_t GPIO_CAN_TX = GPIO_NUM_40;

constexpr adc_oneshot_unit_init_cfg_t adc1UnitConfig_ = { .unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE };

/*
 *	Static Variable Initializations
 */
Core* Core::self_ = nullptr;

/*
 *	Public Function implementations
 */
Core* Core::get()
{
	if (self_ == nullptr) {
		self_ = new Core();
	}

	return self_;
}

void Core::setMainEventQueue(QueueHandle_t queue)
{
	mainEventQueue_ = queue;
}

QueueHandle_t Core::getMainEventQueue() const {
	return mainEventQueue_;
}

std::vector<Display>* Core::getDisplays()
{
	return &displays_;
}

adc_oneshot_unit_handle_t* Core::getAdc()
{
	return &adc1Handle_;
}

Can* Core::getCan() const {
	return can_;
}

/*
 *	Private Function Implementations
 */
Core::Core()
{
	can_ = new Can(GPIO_CAN_RX, GPIO_CAN_TX);
	can_->initialize();
	can_->enable();

	if (adc_oneshot_new_unit(&adc1UnitConfig_, &adc1Handle_) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize ADC1");
	}
}