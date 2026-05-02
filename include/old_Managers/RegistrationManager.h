#pragma once

// C includes
#include <stdbool.h>

/*
 *	Public functions
 */
//! \brief Initializes the registration manager
//! \retval Bool indicating if the initialization was successful
bool registrationManagerInit();

//! \brief Destroys the registration manager
void registrationManagerDestroy();