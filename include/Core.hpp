#pragma once

// Project includes
#include "Can.hpp"
#include "Driver/Display.hpp"

/*
 *	Public constexpr
 */
constexpr uint8_t MASTER_CAN_ID = 1;

constexpr gpio_num_t GPIO_DISPLAY1 = GPIO_NUM_13;
constexpr gpio_num_t GPIO_DISPLAY2 = GPIO_NUM_14;
constexpr gpio_num_t GPIO_DISPLAY3 = GPIO_NUM_21;

/*
 *	Class
 */
class Core
{
public:
	static Core* get();

	void setMainEventQueue(QueueHandle_t queue);

	QueueHandle_t getMainEventQueue() const;

	std::vector<Display>* getDisplays();

	/*
	 *	CAN related functions
	 */
	Can* getCan() const;

private:
	/*
	 *	Instances
	 */
	static Core* self_;

	Can* can_ = nullptr;

	/*
	 *	Private Functions
	 */
	Core();

	/*
	 *	Private Variables
	 */
	QueueHandle_t mainEventQueue_ = nullptr;

	std::vector<Display> displays_ = {
		Display(GPIO_DISPLAY1, MASTER_CAN_ID + 1),
		Display(GPIO_DISPLAY2, MASTER_CAN_ID + 2),
		Display(GPIO_DISPLAY3, MASTER_CAN_ID + 3),
	};
};
