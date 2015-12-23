/**
 * Fallback Motion Capture system for simulating several rigid bodies moving on predefined paths.
 */

#pragma once

#include "MoCapSystem.h"
#include "VectorMath.h"

class MoCapSimulator : public MoCapSystem
{
public:
	MoCapSimulator();
	~MoCapSimulator();

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
	void initBody(int idx);

private:
	bool                   initialised;
	int                    iFrame;
	float                  fTime;
	Vector3D*              arrPos;
	Quaternion*            arrRot;

	bool                   trackingUnreliable;
	int*                   arrTrackingLostCounter;

	sRigidBodyDescription* arrRigidBodyDesc;
};

