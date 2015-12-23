/**
 * Interface for motion capture data providers.
 */

#pragma once

#include "MoCapData.h"
#include <string>

// these two functions can be used by the class to stream frames
extern void signalNewFrame();


class MoCapSystem
{
public:
	virtual bool initialise() = 0;
	virtual bool isActive() = 0;
	virtual int  getUpdateInterval() = 0;
	virtual bool update() = 0;
	virtual bool getSceneDescription(MoCapData& refData) = 0;
	virtual bool getFrameData(MoCapData& refData) = 0;
	virtual bool processCommand(const std::string& strCommand) = 0;
	virtual bool deinitialise() = 0;
};
