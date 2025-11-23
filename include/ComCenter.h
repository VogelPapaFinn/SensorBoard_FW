#pragma once

// espidf includes
#include "esp_timer.h"

// FreeRTOS includes
#include "freertos/FreeRTOS.h"

// C includes

// Defines

/*
 *	Static Variables
 */
//! \brief The handler to the request HW UUIDs timer
extern esp_timer_handle_t uuidTimerHandle;
//! \brief Contains the configuration of the uuidTimerHandle_
extern const esp_timer_create_args_t uuidTimerConf;


/*
 *  Functions
 */

//! \brief Starts and registers the CAN bus message distributor
//! \retval Boolean indicating if the start was successful
bool startCommunicationCenter();
