/**
 * Motion Capture system that uses Cortex
 */

#pragma once

#include "Config.h"

#ifdef USE_CORTEX

#pragma comment(lib, "Cortex_SDK.lib")

#include "MoCapSystem.h"
#include "Cortex.h"


class MoCapCortex : public MoCapSystem
{
public:
	MoCapCortex(const std::string &strCortexAddress, const std::string &strLocalAddress);
	~MoCapCortex();

public:
	virtual bool initialise();
	virtual bool isActive();
	virtual int  getUpdateInterval();
	virtual bool update();
	virtual bool getSceneDescription(MoCapData& refData);
	virtual bool getFrameData(MoCapData& refData);
	virtual bool processCommand(const std::string& strCommand);
	virtual bool deinitialise();

private:
	bool getMoCapSceneDescription();

private:
	bool         initialised;
    std::string  strCortexAddress;
	std::string  strLocalAddress;

	sHostInfo*   pCortexInfo;
};

#endif // #ifdef USE_CORTEX