#pragma once

// C++ includes
#include <vector>

// espidf includes
#include "esp_adc/adc_oneshot.h"

/*
 *	Prototypes to prevent circular inclusion
 */
class Can;
class Filesystem;
class Config;
class Wifi;
class KLine;
class Display;
class WebInterface;

/**
 * \brief Represents the system context containing various core components.
 */
struct SystemContext
{
	//! The CAN bus interface
	Can* can = nullptr;

	//! The filesystem interface
	Filesystem* filesystem = nullptr;

	//! The configuration interface
	Config* config = nullptr;

	std::vector<Display*> displays;

	adc_oneshot_unit_handle_t adc1 = nullptr;

	Wifi* wifi = nullptr;

	KLine * kline = nullptr;

	WebInterface* webInterface = nullptr;
};