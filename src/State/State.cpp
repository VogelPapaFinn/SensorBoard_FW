#include "State/State.hpp"

/*
 *	Public Function Implementations
 */
State::State(TYPE type)
{
	type_ = type;

	core_ = Core::get();
}
