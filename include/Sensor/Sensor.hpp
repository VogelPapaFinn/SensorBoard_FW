#pragma once

class Sensor
{
public:
	virtual ~Sensor() = default;

	virtual int get() { return 0; }

protected:
	virtual void notifyAboutNewValue() {}
};