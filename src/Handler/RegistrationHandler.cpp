#include "Handler/RegistrationHandler.hpp"

// Project includes
#include "Events.hpp"
#include "CanGroupsAndFunctions.hpp"
#include "Driver/Display.hpp"

// espidf includes
#include "esp_event.h"
#include "esp_log.h"

/*
 *	constexpr
 */
constexpr auto TAG = "RegistrationHandler";

/*
 *	Public function implementations
 */
RegistrationHandler::RegistrationHandler(SystemContext* p_sysCon)
{
	sysCon_ = p_sysCon;

	/*
	 *	Register event handlers
	 */
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, CAN_FRAME_RECEIVED,
		[](void* p_handler, esp_event_base_t, int32_t, void* p_payload)
		{
			/*
			 *	Get the instance
			 */
			if (p_handler == nullptr) {
				return;
			}

			RegistrationHandler* handler = static_cast<RegistrationHandler*>(p_handler);

			/*
			 *	Get the payload
			 */
			if (p_payload == nullptr) {
				return;
			}

			Can::Frame* frame = static_cast<Can::Frame*>(p_payload);

			/*
			 *	Handle it if its a registration attempt
			 */
			if (frame->group != CanFrameGroups::GROUP::CONFIGURATION ||
				frame->function != CanFrameGroups::CONFIGURATION::REGISTER_AT_MASTER) {
				return;
			}

			handler->handleRegistration(frame);
		},
		this, nullptr);
}

/*
 *	Private Functions Implementations
 */
void RegistrationHandler::handleRegistration(const Can::Frame* p_frame) const
{
	/*
	 *	Get the correct display instance
	 */
	bool displayCrashed = true;
	Display* display = nullptr;
	for (const auto& d : sysCon_->displays) {
		displayCrashed &= d->hasBeenConfigured();


		if (d->getCanId() != p_frame->sender) {
			continue;
		}

		display = d;
	}

	if (display == nullptr) {
		return;
	}

	/*
	 *	Reset the display instance
	 */
	display->reset();

	/*
	 *	Set the screen and rotation
	 */
	const bool correctScreen = p_frame->data[0] == display->getScreen();
	const bool correctRotation = static_cast<bool>(p_frame->data[1]) == display->isRotated();

	// Apply the correct screen & rotation if necessary
	if (!correctScreen || !correctRotation) {
		display->applyScreen();
		display->applyRotation();
	}

	// Confirm the configuration
	display->confirmConfiguration();

	// Turn it on if necessary
	if (displayCrashed) {
		wakeUpAllDisplays();
	}

	/*
	 *	Add event to the event loop
	 */
	esp_event_post(SYSTEM_EVENT_BASE, DISPLAY_REGISTERED, display, sizeof(*display), portMAX_DELAY);
}

void RegistrationHandler::wakeUpAllDisplays() const
{
	Can::Frame txFrame;
	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = CAN_BROADCAST_ID;
	txFrame.group = CanFrameGroups::GROUP::CONFIGURATION;
	txFrame.function = CanFrameGroups::CONFIGURATION::WAKE_UP;
	txFrame.dataLengthCode = 0;
	txFrame.answer = false;

	sysCon_->can->queueFrame(txFrame);
}
