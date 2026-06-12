#pragma once

struct Event
{
	typedef enum
	{
		UNKNOWN,
		REGISTRATION_FINISHED,
		DISPLAY_UPDATE_DOWNLOADED
	} TYPE;

	Event(const TYPE type = UNKNOWN, const int data = 0)
	{
		this->type = type;
		this->data = data;
	}

	TYPE type = UNKNOWN;

	int data = 0;
};