#include "Handler/RegistrationHandler.hpp"

// Project includes
#include "CanGroupsAndFunctions.hpp"
#include "Driver/Display.hpp"
#include "Events.hpp"

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
			if (frame->group != CanFrameGroups::GROUP::CONFIGURATION) {
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
	 *	Act depending on the function
	 */
	switch (p_frame->function) {
		case CanFrameGroups::CONFIGURATION::REGISTER_AT_MASTER:
		{
			// Check payload size
			if (p_frame->dataLengthCode != 3) {
				return;
			}

			/*
			 *	Get the correct display
			 */
			Display* display = nullptr;

			// Iterate through all display instances
			for (const auto& d : sysCon_->displays) {
				// Skip it if its not the one the message came from and it has been configured already
				if (d->getCanId() != p_frame->data[0] && d->hasBeenConfigured()) {
					continue;
				}

				display = d;
				break;
			}

			// If its a nullptr we didnt find the instance
			if (display == nullptr) {
				return;
			}

			/*
			 *	Reset the display instance
			 */
			display->reset();

			/*
			 *	Bake the configuration if needed
			 */
			const auto data = p_frame->data;
			if (data[0] != display->getCanId() || data[1] != display->getScreen() || data[2] != display->isRotated())
			{
				display->bakeConfiguration();
			} else {
				display->confirmConfiguration();
			}

			/*
			 *	Turn it on if it crashed
			 */
			bool crashed = true;

			// If all other are marked as configured, it probably crashed
			for (const auto& d : sysCon_->displays) {
				crashed &= d->hasBeenConfigured();
			}

			if (crashed) {
				wakeUpAllDisplays();
			}

			/*
			 *	Mark the display as configured
			 */
			display->setConfigured(true);

			/*
			 *	Add event to the event loop
			 */
			esp_event_post(SYSTEM_EVENT_BASE, DISPLAY_REGISTERED, display, sizeof(*display), portMAX_DELAY);

		} break;

		default: break;
	}
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
