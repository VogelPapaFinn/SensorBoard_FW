#include "Driver/Display.hpp"

// Project includes
#include "Can.hpp"
#include "CanGroupsAndFunctions.hpp"

/*
 *	Static Member Variables
 */
std::vector<uint8_t> Display::g_possibleCanIds = {1, 2, 3};

/*
 *	constexpr
 */
constexpr auto TAG = "Display";


/*
 *	Public function implementations
*/
Display::Display(SystemContext* p_sysCon, const gpio_num_t powerGpio, const uint8_t& canId, const uint8_t& screen, const bool& rotateBy180)
{
	sysCon_ = p_sysCon;

	powerGpio_ = powerGpio;
	if (powerGpio_ != GPIO_NUM_NC) {
		gpio_set_direction(powerGpio_, GPIO_MODE_OUTPUT);
		gpio_set_level(powerGpio_, 0);
	}

	canId_ = canId;
	screen_ = screen;
	rotated_ = rotateBy180;
}

void Display::reset()
{
	configured_ = false;
}

bool Display::hasBeenConfigured() const
{
	return configured_;
}

uint8_t Display::getCanId() const
{
	return canId_;
}

void Display::applyScreen() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = canId_;
	txFrame.group = CanFrameGroups::GROUP::CONFIGURATION;
	txFrame.function = CanFrameGroups::CONFIGURATION::SET_SCREEN;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = screen_;
	txFrame.answer = false;

	sysCon_->can->queueFrame(txFrame);
}

uint8_t Display::getScreen() const
{
	return screen_;
}

void Display::applyRotation() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = canId_;
	txFrame.group = CanFrameGroups::GROUP::CONFIGURATION;
	txFrame.function = CanFrameGroups::CONFIGURATION::SET_ROTATION;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = rotated_;
	txFrame.answer = false;

	sysCon_->can->queueFrame(txFrame);
}

bool Display::isRotated() const
{
	return rotated_;
}

void Display::confirmConfiguration()
{
	configured_ = true;

	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = canId_;
	txFrame.group = CanFrameGroups::GROUP::CONFIGURATION;
	txFrame.function = CanFrameGroups::CONFIGURATION::CONFIRM_CONFIGURATION;
	txFrame.answer = false;

	sysCon_->can->queueFrame(txFrame);
}

void Display::turnOn() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, 1);
}

void Display::turnOff() const
{
	if (powerGpio_ == GPIO_NUM_NC) {
		return;
	}

	gpio_set_level(powerGpio_, 0);
}
