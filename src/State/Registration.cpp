#include "State/Registration.hpp"

// Project includes
#include "CanGroupsAndFunctions.hpp"
#include "Driver/Display.hpp"
#include "Events.hpp"

// espidf includes
#include "esp_log.h"

/*
 *	constexpr
 */
const auto TAG = "Registration";

/*
 *	Public Function Implementations
 */
Registration::Registration(SystemContext* p_sysCon) : State(State::REGISTRATION) { sysCon_ = p_sysCon; }

Registration::~Registration()
{
	/*
	 *	Unregister from all events
	 */
	for (const auto& event : eventHandlers_) {
		const auto& base = std::get<0>(event);
		const auto& id = std::get<1>(event);
		const auto& handler = std::get<2>(event);

		esp_event_handler_instance_unregister(base, id, handler);
	}
	eventHandlers_.clear();
}

void Registration::enter()
{
	// Register necessary events on the event loop
	registerToEvents();

	// Start the first display
	sysCon_->displays.at(0)->turnOn();
}

/*
 *	Private Function Implementations
 */
void Registration::registerToEvents()
{
	/*
	 *	Display registered
	 */
	eventHandlers_.push_back(std::make_tuple(SYSTEM_EVENT_BASE, DISPLAY_REGISTERED, esp_event_handler_instance_t()));
	esp_event_handler_instance_register(
		SYSTEM_EVENT_BASE, DISPLAY_REGISTERED,
		[](void* p_state, esp_event_base_t, int32_t, void*)
		{
			/*
			 *	Get the state ptr
			 */
			if (p_state == nullptr) {
				return;
			}

			// Convert it
			Registration* state = static_cast<Registration*>(p_state);

			/*
			 *	Pass the register call
			 */
			state->nextDisplay();
		},
		this, &get<2>(eventHandlers_.back()));
}

void Registration::nextDisplay()
{
	ESP_LOGI(TAG, "Next Display");

	/*
	 *	Complete registration if possible
	 */
	if (++currDisplay >= 3) {
		wakeUpAllDisplays();

		esp_event_post(SYSTEM_EVENT_BASE, REGISTRATION_COMPLETED, nullptr, 0, portMAX_DELAY);

		return;
	}

	/*
	 *	Turn on the next display
	 */
	sysCon_->displays.at(currDisplay)->turnOn();
}

void Registration::wakeUpAllDisplays() const
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
