#pragma once
#include "Config.h"

#ifdef USE_KINECT

#pragma comment(lib, "Kinect10.lib")

#include "MoCapSystem.h"
#include "SystemConfiguration.h"

#include <Windows.h>
#include <NuiApi.h>

#include <vector>


class MoCapKinectConfiguration : public SystemConfiguration
{
public:
	MoCapKinectConfiguration();

	virtual bool handleParameter(int idx, const std::string& value);

public:

	bool  useKinect;
	bool  seatedMode;
};


class MoCapKinect : public MoCapSystem
{
public:
	MoCapKinect(MoCapKinectConfiguration configuration);
	virtual ~MoCapKinect();

public:
	virtual bool  initialise();
	virtual bool  isActive();
	virtual float getUpdateRate();
	virtual bool  isRunning();
	virtual void  setRunning(bool running);
	virtual bool  update();
	virtual bool  getSceneDescription(MoCapData& refData);
	virtual bool  getFrameData(MoCapData& refData);
	virtual bool  processCommand(const std::string& strCommand);
	virtual bool  deinitialise();

private:

	void handleSkeletonData(const NUI_SKELETON_FRAME& refSkeletonFrame, MoCapData& refData);
	void cleanup();

private:

	MoCapKinectConfiguration configuration;

	bool        initialised;
	bool        running;
	INuiSensor* pNuiSensor;
	HANDLE      kinectHandle;
};

#endif
