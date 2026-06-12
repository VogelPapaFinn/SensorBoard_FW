#include "State/State.hpp"

/*
 *	Public Function Implementations
 */
State::State(TYPE type)
{
	type_ = type;

	core_ = Core::get();
}

State::TYPE State::getType() const
{
	return type_;
}
