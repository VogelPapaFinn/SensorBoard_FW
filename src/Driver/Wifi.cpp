#include "Driver/Wifi.hpp"

// Project includes
#include "Core.hpp"

/*
 *	constexpr
 */
constexpr auto TAG = "Wifi";

/*
 *	Public Function Implementations
 */
Wifi::Wifi(WIFI_TYPE type) : type_(type)
{
	core_ = Core::get();
}

void Wifi::callOnSuccess(const std::function<void()>& cb)
{
	callOnSuccess_ = cb;
}

void Wifi::setSSID(const std::string ssid)
{
	ssid_ = ssid;
}

void Wifi::setPassword(const std::string password)
{
	password_ = password;
}

std::string Wifi::getSSID()
{
	return ssid_;
}

std::string Wifi::getPassword()
{
	return password_;
}
