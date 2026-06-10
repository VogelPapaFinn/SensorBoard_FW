#include "State/WifiOtaUpdate.hpp"

/*
 *	constexpr
 */
const auto TAG = "WifiOtaUpdate";

/*
 *	Public Function Implementations
 */
WifiOtaUpdate::WifiOtaUpdate() :
	State(State::WIFI_OTA_UPDATE)
{
}

WifiOtaUpdate::~WifiOtaUpdate()
{
}

void WifiOtaUpdate::enter()
{

}

/*
 *	Private Function Implementations
 */
void WifiOtaUpdate::broadcastWifiSSID()
{
	Can::Frame txFrame;

	txFrame.sender = CAN_MASTER_ID;
	txFrame.target = CAN_BROADCAST_ID;
	txFrame.group = CanFrame::GROUP::WIFI;
	txFrame.function = CanFrame::WIFI::SET_SSID;
	txFrame.dataLengthCode = 8;

	core_->getCan()->queueFrame(txFrame);
}
