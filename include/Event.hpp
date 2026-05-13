#pragma once

struct Event
{
	typedef enum
	{
		UNKNOWN,
		REGISTRATION_FINISHED,
	} TYPE;

	Event(const TYPE type = UNKNOWN, const int data = 0)
	{
		this->type = type;
		this->data = data;
	}

	TYPE type = UNKNOWN;

	int data = 0;
};