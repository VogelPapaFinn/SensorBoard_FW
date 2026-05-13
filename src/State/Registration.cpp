#include "State/Registration.hpp"

// Project includes
#include "Event.hpp"

/*
 *	Public Function Implementations
 */
Registration::Registration() :
	State(State::REGISTRATION)
{
}

void Registration::enter()
{
}

void Registration::handleCanFrame(const Can::Frame& frame)
{
	if (frame.group != CanFrame::GROUP::CONFIGURATION) {
		return;
	}

	if (frame.target != CAN_MASTER_ID) {
		return;
	}

	esp_rom_printf("Received Frame %s\n", frame.toString().c_str());

	const uint8_t& expectedCanId = core_->getDisplays()->at(currDisplay).getCanId();

	// Act depending on the function type
	switch (frame.function) {
		case CanFrame::REGISTER_AT_MASTER:
		{
			// Correct ID
			if (frame.sender == expectedCanId) {
				confirmId(currDisplay);
				setScreen();
				return;
			}

			// Wrong ID
			setId(frame.sender, expectedCanId);
			return;
		}
		break;

		case CanFrame::SET_ID:
		{
			if (!frame.answer) {
				return;
			}

			// Matches now
			if (frame.sender == expectedCanId) {
				confirmId(expectedCanId);
				setScreen();
				return;
			}

			// Still doesn't match
			setId(frame.sender, expectedCanId);
			return;
		}
		break;

		case CanFrame::SET_SCREEN:
		{
			if (!frame.answer) {
				return;
			}

			nextDisplay();
		}
		break;

		default:
		{
			esp_rom_printf("DEFAULT\n");
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
	if (++currDisplay >= 1) {
		wakeUpAllDisplays();

		Event event(Event::REGISTRATION_FINISHED);
		xQueueSend(core_->getMainEventQueue(), &event, portMAX_DELAY);

		return;
	}

	core_->getDisplays()->at(currDisplay).turnOn();
}

void Registration::setScreen()
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = core_->getDisplays()->at(currDisplay).getCanId();
	txFrame.group = CanFrame::GROUP::CONFIGURATION;
	txFrame.function = CanFrame::CONFIGURATION::SET_SCREEN;
	txFrame.dataLengthCode = 1;
	txFrame.data[0] = 1;
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
