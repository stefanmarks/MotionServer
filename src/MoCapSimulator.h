/**
 * Motion Capture system for simulating several rigid bodies moving on predefined paths.
 */

#pragma once

#include "MoCapSystem.h"
#include "VectorMath.h"

#include <vector>


class MoCapSimulator : public MoCapSystem
{
public:
	MoCapSimulator();
	virtual ~MoCapSimulator();

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
	bool                    initialised;
	bool                    isPlaying;
	int                     iFrame;
	float                   fTime;
	std::vector<Vector3D>   arrPos;
	std::vector<Quaternion> arrRot;

	bool                    trackingUnreliable;
	std::vector<int>        arrTrackingLostCounter;
};

