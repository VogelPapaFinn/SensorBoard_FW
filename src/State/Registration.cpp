#include "State/Registration.hpp"

// Project includes
#include "Event.hpp"

/*
 *	constexpr
 */
const auto TAG = "Registration";

/*
 *	Public Function Implementations
 */
Registration::Registration() :
	State(State::REGISTRATION)
{
}

void Registration::enter()
{
	core_->getDisplays()->at(0).turnOn();
}

void Registration::handleCanFrame(const Can::Frame& frame)
{
	if (blocked) {
		return;
	}

	if (frame.group != CanFrame::GROUP::CONFIGURATION) {
		return;
	}

	if (frame.target != CAN_MASTER_ID) {
		return;
	}

	const uint8_t& expectedCanId = core_->getDisplays()->at(currDisplay).getCanId();

	// Act depending on the function type
	switch (frame.function) {
		case CanFrame::REGISTER_AT_MASTER:
		{
			const bool correctCanId = frame.sender == expectedCanId;
			const bool correctScreen = frame.data[0] == core_->getDisplays()->at(currDisplay).getScreen();
			const bool correctRotation = static_cast<bool>(frame.data[1]) == core_->getDisplays()->at(currDisplay).isRotated();

			if (!correctCanId || !correctScreen || !correctRotation) {
				setId(frame.sender, expectedCanId);
				setScreen();
				setRotation();
			}

			confirmConfiguration();
			nextDisplay();
		}
		break;

		default:
		{
			esp_rom_printf("Received unknown CAN message type!\n");
		}
		break;
	}
}

/*
 *	Private Function Implementations
 */
void Registration::confirmId(const uint8_t& id)
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = id;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::CONFIRM_ID;
	txFrame.answer = true;

	Core::get()->getCan()->queueFrame(txFrame);
}

void Registration::setId(const uint8_t& oldId, const uint8_t& newId)
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = oldId;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ID;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = newId;
	txFrame.answer = false;

	Core::get()->getCan()->queueFrame(txFrame);
}

void Registration::nextDisplay()
{
	ESP_LOGI(TAG, "Next Display");

	if (++currDisplay >= 3) {
		wakeUpAllDisplays();

		Event event(Event::REGISTRATION_FINISHED);
		xQueueSend(core_->getMainEventQueue(), &event, portMAX_DELAY);
		blocked = true;

		return;
	}

	core_->getDisplays()->at(currDisplay).turnOn();
}

void Registration::setScreen() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = core_->getDisplays()->at(currDisplay).getCanId();
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_SCREEN;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = core_->getDisplays()->at(currDisplay).getScreen();
	txFrame.answer = false;

	Core::get()->getCan()->queueFrame(txFrame);
}

void Registration::setRotation() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = core_->getDisplays()->at(currDisplay).getCanId();
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_ROTATION;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = core_->getDisplays()->at(currDisplay).isRotated();
	txFrame.answer = false;

	Core::get()->getCan()->queueFrame(txFrame);
}

void Registration::confirmConfiguration() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = core_->getDisplays()->at(currDisplay).getCanId();
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::CONFIRM_CONFIGURATION;
	txFrame.answer = false;

	Core::get()->getCan()->queueFrame(txFrame);
}

void Registration::wakeUpAllDisplays()
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = CAN_BROADCAST_ID;
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::WAKE_UP;
	txFrame.dataLengthCode = 0;
	txFrame.answer = false;

	Core::get()->getCan()->queueFrame(txFrame);
}
