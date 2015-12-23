#include "MoCapSimulator.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapSimulator"

#include "stdlib.h"
#include "stdio.h"
#include "math.h"
#include "memory.h"


const int _frameRate = 60;


struct sRigidBodyMovementParams
{
	char* szName;
	int   axis;
	float radius;
	float posOffset;
	float rotOffset;
	float speed;
};

const sRigidBodyMovementParams RIGID_BODY_PARAMS[] =
{	{ "Walk_1m",  1,   -1,  1.5, -25, 1.0f /  15 }, // negative radius to make Z-axis face inwards, looking down towards origin
	{ "Walk_2m",  1,   -2,  1.5, -20, 1.0f /  20 }, // "
	{ "Walk_3m",  1,   -3,  1.5, -15, 1.0f / -25 }, // "
	{ "Walk_4m",  1,   -4,  1.5, -10, 1.0f / -30 }, // "
	{ "Walk_5m",  1,   -5,  1.5,  -7, 1.0f /  40 }, // "
	{ "Walk_10m", 1,  -10,  1.5,  -5, 1.0f /  50 }, // "
	{ "Oculus",   1,   -3,  1.5, -15, 1.0f /  30 },
	{ "RotX+",    0,  0.5,  1.0,   0, 1.0f /  10 },
	{ "RotX-",    0, -0.5, -1.0,   0, 1.0f / -10 },
	{ "RotY+",    1,  0.5,  1.0,   0, 1.0f /  10 },
	{ "RotY-",    1, -0.5, -1.0,   0, 1.0f / -10 },
	{ "RotZ+",    2,  0.5,  1.0,   0, 1.0f /  10 },
	{ "RotZ-",    2, -0.5, -1.0,   0, 1.0f / -10 },
};

const int RIGID_BODY_COUNT = sizeof(RIGID_BODY_PARAMS) / sizeof(RIGID_BODY_PARAMS[0]);
const int SKELETON_COUNT   = 0;


MoCapSimulator::MoCapSimulator() :
	initialised(false)
{
}


bool MoCapSimulator::initialise()
{
	if (!initialised)
	{
		fTime = 0;
		iFrame = 0;

		arrPos = new Vector3D[RIGID_BODY_COUNT];
		arrRot = new Quaternion[RIGID_BODY_COUNT];

		arrTrackingLostCounter = new int[RIGID_BODY_COUNT];
		for (int i = 0; i < RIGID_BODY_COUNT; i++)
		{
			arrTrackingLostCounter[i] = 0;
		}
		trackingUnreliable = false;

		LOG_INFO("Initialised");

		initialised = true;
	}
	return initialised;
}


bool MoCapSimulator::isActive()
{
	return initialised;
}


int MoCapSimulator::getUpdateInterval()
{
	return 1000 / _frameRate;
}


bool MoCapSimulator::update()
{
	iFrame += 1;
	fTime  += (1.0f / _frameRate);

	for (int b = 0; b < RIGID_BODY_COUNT; b++)
	{
		// calculate new positions/rotations
		float t    = fTime * RIGID_BODY_PARAMS[b].speed;
		float r    = RIGID_BODY_PARAMS[b].radius;
		float oPos = RIGID_BODY_PARAMS[b].posOffset;
		float oRot = RIGID_BODY_PARAMS[b].rotOffset;
		switch (RIGID_BODY_PARAMS[b].axis)
		{
			case 0:
			{
				arrPos[b].set(oPos, r * cos(t), r * sin(t));   // zero degrees = Y+ up
				arrRot[b].fromAxisAngle(1, 0, 0, t);
				break;
			}
			case 1:
			{
				arrPos[b].set(r * -sin(t), oPos, r * -cos(t)); // zero degrees = Z- forwards
				arrRot[b].fromAxisAngle(0, 1, 0, t);
				Quaternion rotX(1, 0, 0, oRot * (float) (M_PI / 180));
				arrRot[b] = arrRot[b].mult(rotX);
				break;
			}
			case 2:
			{
				arrPos[b].set(r * -sin(t), r * cos(t), oPos);  // zero degrees = Y+ upwards
				arrRot[b].fromAxisAngle(0, 0, 1, t);
				break;
			}
		}
		/*
		sBodyData& data = frameData.BodyData[i];

		// update marker data
		for (int m = 0; m < data.nMarkers; m++)
		{
			data.Markers[m][0] = arrPos[i].x + (rand() * 0.1f / RAND_MAX - 0.05f);
			data.Markers[m][1] = arrPos[i].y + (rand() * 0.1f / RAND_MAX - 0.05f);
			data.Markers[m][2] = arrPos[i].z + (rand() * 0.1f / RAND_MAX - 0.05f);
		}

		// update rigid body data
		tSegmentData& seg = data.Segments[0];
		seg[0] = arrPos[i].x;
		seg[1] = arrPos[i].y;
		seg[2] = arrPos[i].z;
		seg[3] = arrRot[i].x;
		seg[4] = arrRot[i].y;
		seg[5] = arrRot[i].z;
		*/

		if (trackingUnreliable)
		{
			if (rand() < RAND_MAX / 1000)
			{
				arrTrackingLostCounter[b] = rand() * 100 / RAND_MAX;
			}
		}
	}

	/*
	for (int s = 0; s < SKELETON_COUNT; s++)
	{

	}
	*/

	signalNewFrame();

	return true;
}


bool MoCapSimulator::getSceneDescription(MoCapData& refData)
{
	LOG_INFO("Requesting scene description")

	int descrIdx = 0;
	for (int b = 0; b < RIGID_BODY_COUNT; b++)
	{
		sMarkerSetDescription* pMarkerDesc = new sMarkerSetDescription();
		// fill in description structure
		pMarkerDesc->nMarkers = 0;
		strcpy_s(pMarkerDesc->szName, sizeof(pMarkerDesc->szName), RIGID_BODY_PARAMS[b].szName);
		
		// pre-fill in frame structure for marker sets
		sMarkerSetData& msData = refData.frame.MocapData[b];
		msData.nMarkers = 0;
		strcpy_s(msData.szName, sizeof(msData.szName), pMarkerDesc->szName);

		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_MarkerSet;
		refData.description.arrDataDescriptions[descrIdx].Data.MarkerSetDescription = pMarkerDesc;
		descrIdx++;

		sRigidBodyDescription* pBodyDesc = new sRigidBodyDescription();
		// fill in description structure
		pBodyDesc->ID       = b; // needs to be equal to array index
		pBodyDesc->parentID = -1;
		pBodyDesc->offsetx  = 0;
		pBodyDesc->offsety  = 0;
		pBodyDesc->offsetz  = 0;
		strcpy_s(pBodyDesc->szName, sizeof(pBodyDesc->szName), pMarkerDesc->szName);

		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_RigidBody;
		refData.description.arrDataDescriptions[descrIdx].Data.RigidBodyDescription = pBodyDesc;
		descrIdx++;
	}

	for (int s = 0; s < SKELETON_COUNT; s++)
	{
		sSkeletonDescription* pSkeleton = new sSkeletonDescription();
		// fill in description structure
		pSkeleton->nRigidBodies = 2;
		//strcpy_s(pSkeleton->szName, ...);
		
		// pre-fill in frame structure for marker sets
		sSkeletonData& skData = refData.frame.Skeletons[s];
//todo

		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_Skeleton;
		refData.description.arrDataDescriptions[descrIdx].Data.SkeletonDescription = pSkeleton;
		descrIdx++;
	}
	
	refData.description.nDataDescriptions = descrIdx;

	// pre-fill in frame data
	refData.frame.nMarkerSets  = RIGID_BODY_COUNT;
	refData.frame.nRigidBodies = RIGID_BODY_COUNT;
	refData.frame.nSkeletons   = SKELETON_COUNT;

	refData.frame.nOtherMarkers = 0;
	refData.frame.OtherMarkers  = NULL;

	refData.frame.nLabeledMarkers = 0;

	refData.frame.nForcePlates = 0;

	refData.frame.fLatency = 0;
	refData.frame.Timecode = 0;
	refData.frame.TimecodeSubframe = 0;

	return true;
}


bool MoCapSimulator::getFrameData(MoCapData& refData)
{
	refData.frame.iFrame = iFrame;

	for (int b = 0; b < RIGID_BODY_COUNT; b++)
	{
		bool trackingLost = false;

		if (arrTrackingLostCounter[b] > 0)
		{
			trackingLost = true;
			arrTrackingLostCounter[b]--;
		}

		sRigidBodyData& rbData = refData.frame.RigidBodies[b];
		rbData.ID = b;
		rbData.x  = trackingLost ? 0 : arrPos[b].x;
		rbData.y  = trackingLost ? 0 : arrPos[b].y;
		rbData.z  = trackingLost ? 0 : arrPos[b].z;
		rbData.qx = trackingLost ? 0 : arrRot[b].x;
		rbData.qy = trackingLost ? 0 : arrRot[b].y;
		rbData.qz = trackingLost ? 0 : arrRot[b].z;
		rbData.qw = trackingLost ? 0 : arrRot[b].w;

		rbData.nMarkers  = 0;
		rbData.MeanError = 0;
	}

	return true;
}


bool MoCapSimulator::processCommand(const std::string& strCommand)
{
	bool processed = false;
	
	if ( strCommand == "u" )
	{
		trackingUnreliable = !trackingUnreliable;
		LOG_INFO("Tracking " << (trackingUnreliable ? "unreliable" : "reliable"));
		processed = true;
	}

	return processed;
}


bool MoCapSimulator::deinitialise()
{
	if (initialised)
	{
		delete[] arrPos; arrPos = NULL;
		delete[] arrRot; arrRot = NULL;

		LOG_INFO("Deinitialised");

		initialised = false;
	}
	return !initialised;
}


MoCapSimulator::~MoCapSimulator()
{
	deinitialise();
}

