#pragma once
#include "Config.h"

#ifdef USE_KINECT

#pragma comment(lib, "Kinect10.lib")

#include <Windows.h>
#include "MoCapSystem.h"
#include "Configuration.h"

#include <NuiApi.h>

#include <vector>


class MoCapKinectConfiguration : public Configuration
{
public:
	MoCapKinectConfiguration();

	virtual bool handleArgument(unsigned int _idx, const std::string& _value);

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

	void  handleSkeletonData(const NUI_SKELETON_FRAME& refSkeletonFrame, MoCapData& refData);
	float calculateBoneLength(int aindex, NUI_SKELETON_DATA skeleton);
	float calculateBoneOffset(int aindex, NUI_SKELETON_DATA skeleton);
	void  checkUserLost(const NUI_SKELETON_FRAME& refSkeletonFrame);
	void  checkUserFound(const NUI_SKELETON_FRAME& refSkeletonFrame);
	void  readRigidBodyDescription(sRigidBodyDescription& descr, sRigidBodyData& data, int rbodies);
	void  cleanup();

private:

	MoCapKinectConfiguration configuration;

	bool             initialised;
	bool             running;
	INuiSensor*      pNuiSensor;
	HANDLE           kinectHandle;

	std::vector<int> userSkeletonIdx;
};

#endif
