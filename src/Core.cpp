#include "Core.hpp"

/*
 *	constexpr
 */
constexpr auto TAG = "Core";

constexpr gpio_num_t GPIO_CAN_RX = GPIO_NUM_41;
constexpr gpio_num_t GPIO_CAN_TX = GPIO_NUM_40;

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

Can* Core::getCan() const {
	return can_;
}

/*
 *	Private Function Implementations
 */
Core::Core()
{
	/*
	 *	Backend
	 */
	can_ = new Can(GPIO_CAN_RX, GPIO_CAN_TX);

	can_->initialize();
	can_->enable();

}