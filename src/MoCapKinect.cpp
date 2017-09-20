#include "MocapKinect.h"

#ifdef USE_KINECT

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapKinect"

#include "VectorMath.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <math.h>


#define MAX_USERS        NUI_SKELETON_MAX_TRACKED_COUNT
#define USER_NOT_TRACKED -1


/**
* Structure for associating marker names with their Kinect skeleton ID
*/
struct sKinectSkeletonData
{
	const char*                       czPositionName;
	const NUI_SKELETON_POSITION_INDEX index;
	const int boneIndex;
};

struct sKinectBoneData
{
	const char*                       czBoneName;
	const int parentIndex;
	const NUI_SKELETON_POSITION_INDEX kParent;
	const NUI_SKELETON_POSITION_INDEX kPoint;
	const NUI_SKELETON_POSITION_INDEX kEnd;
};

const sKinectBoneData BONE_DATA[]
{
	{ "Hip", -1, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER},
	{ "Spine",  0, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE},
	{ "Neck" , 1, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER },
	{ "Head",  2, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD},
	{ "ClavicleLeft",   2, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT},
	{ "UpperArmLeft",   4, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT},
	{ "LowerArmLeft",   5, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT},
	{ "HandLeft",      6, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT},
	{ "ClavicleRight", 2, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT},
	{ "UpperArmRight",    8, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT},
	{ "LowerArmRight",    9, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT},
	{ "HandRight",     10, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT},
	{ "HipLeft",       0, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT},
	{ "UpperLegLeft",      12, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT},
	{ "LowerLegLeft",     13, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT},
	{ "FootLeft",      14, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT},
	{ "HipRight",      0, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT},
	{ "UpperLegRight",   16, NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT},
	{ "LowerLegRight",    17, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT},
	{ "FootRight",     18, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT},
};

const sKinectSkeletonData SKELETON_DATA[]
{
	{ "HipCentre",     NUI_SKELETON_POSITION_HIP_CENTER, 0},
	{ "Spine",         NUI_SKELETON_POSITION_SPINE, 1},
	{ "ShoulderCentre",NUI_SKELETON_POSITION_SHOULDER_CENTER, 2 },
	{ "Head",          NUI_SKELETON_POSITION_HEAD, 3 },
	{ "ShoulderLeft",  NUI_SKELETON_POSITION_SHOULDER_LEFT, 4 },
	{ "ElbowLeft",     NUI_SKELETON_POSITION_ELBOW_LEFT, 5 },
	{ "WristLeft",     NUI_SKELETON_POSITION_WRIST_LEFT, 6 },
	{ "HandLeft",      NUI_SKELETON_POSITION_HAND_LEFT, 7 },
	{ "ShoulderRight", NUI_SKELETON_POSITION_SHOULDER_RIGHT, 8 },
	{ "ElbowRight",    NUI_SKELETON_POSITION_ELBOW_RIGHT, 9 },
	{ "WristRight",    NUI_SKELETON_POSITION_WRIST_RIGHT, 10 },
	{ "HandRight",     NUI_SKELETON_POSITION_HAND_RIGHT, 11 },
	{ "HipLeft",       NUI_SKELETON_POSITION_HIP_LEFT, 12},
	{ "KneeLeft",      NUI_SKELETON_POSITION_KNEE_LEFT, 13 },
	{ "AnkleLeft",     NUI_SKELETON_POSITION_ANKLE_LEFT, 14 },
	{ "FootLeft",      NUI_SKELETON_POSITION_FOOT_LEFT, 15 },
	{ "HipRight",      NUI_SKELETON_POSITION_HIP_RIGHT, 16},
	{ "KneeRight",     NUI_SKELETON_POSITION_KNEE_RIGHT, 17},
	{ "AnkleRight",    NUI_SKELETON_POSITION_ANKLE_RIGHT, 18},
	{ "FootRight",     NUI_SKELETON_POSITION_FOOT_RIGHT, 19},
};

const int SKELETON_DATA_COUNT = sizeof(SKELETON_DATA) / sizeof(SKELETON_DATA[0]);
const int BONE_DATA_COUNT = sizeof(BONE_DATA) / sizeof(BONE_DATA[0]);



/******************************************************************************
 * MoCapKinectConfiguration class
 */

MoCapKinectConfiguration::MoCapKinectConfiguration() :
	Configuration("Kinect"),
	useKinect(false),
	seatedMode(false)
{
	addOption("-useKinect",  "Search for and use a Kinect sensor if connected");
	addOption("-seatedMode", "Do not track the legs and feet");
}


bool MoCapKinectConfiguration::handleArgument(unsigned int _idx, const std::string& _value)
{
	bool success = true;
	switch (_idx)
	{
		case 0:
			useKinect = true;
			break;

		case 1:
			seatedMode = true;
			break;

		default:
			success = false;
			break;
	}
	return success;
}



/******************************************************************************
 * MoCapKinect class
 */

MoCapKinect::MoCapKinect(MoCapKinectConfiguration configuration) :
	configuration(configuration),
	initialised(false),
	running(true),
	pNuiSensor(nullptr)
{
	// assume no tracked users in the beginning
	for (int userIdx = 0; userIdx < MAX_USERS; userIdx++)
	{
		userSkeletonIdx.push_back(USER_NOT_TRACKED);
	}
}


bool MoCapKinect::initialise()
{
	if (!initialised)
	{
		// try to find a sensor at all
		HRESULT result;
		result = NuiCreateSensorByIndex(0, &pNuiSensor);
		if (SUCCEEDED(result))
		{
			LOG_INFO("Found at least one Kinect sensor");
			// found it > initialise
			result = pNuiSensor->NuiInitialize(
				NUI_INITIALIZE_FLAG_USES_SKELETON);
		}
		else
		{
			LOG_WARNING("Cannot find any Kinect sensor");
			cleanup();
		}

		if (SUCCEEDED(result))
		{
			// initialised sensor > enable skeletal tracking
			LOG_INFO("Initialised Kinect sensor");
			kinectHandle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			result = pNuiSensor->NuiSkeletonTrackingEnable(kinectHandle,
				configuration.seatedMode ? NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT : 0
			);
		}
		else
		{
			LOG_INFO("Cannot initialise Kinect sensor");
			cleanup();
		}

		if (SUCCEEDED(result))
		{
			LOG_INFO("Kinect sensor initialized" << (configuration.seatedMode ? " (seated mode)" : ""));
			initialised = true;
		}
		else
		{
			LOG_INFO("Cannot enable skeleton tracking");
			cleanup();
		}
	}
	return initialised;
}


bool MoCapKinect::isActive()
{
	return initialised;
}


float MoCapKinect::getUpdateRate()
{
	// Kinect 1 runs at 30 fps
	return 30;
}


bool MoCapKinect::getFrameData(MoCapData& refData)
{
	// update marker data
	if (running && (WAIT_OBJECT_0 == WaitForSingleObject(kinectHandle, 0)))
	{
		NUI_SKELETON_FRAME skeletonFrame = { 0 };

		if (SUCCEEDED(pNuiSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame)))
		{
			// get the frame number and timestamp
			refData.frame.iFrame     = (int)skeletonFrame.dwFrameNumber;
			refData.frame.fTimestamp = skeletonFrame.liTimeStamp.LowPart / 1000.0f; // convert milliseconds to seconds

			// handle the skeleton data
			handleSkeletonData(skeletonFrame, refData);
		}
	}

	return running;
}


bool MoCapKinect::getSceneDescription(MoCapData& refData)
{
	int descrIdx = 0;

	for (int userIdx = 0; userIdx < MAX_USERS; userIdx++)
	{
		// create markerset description and frame
		sMarkerSetDescription* pMarkerDesc = new sMarkerSetDescription();
		sMarkerSetData&        msData = refData.frame.MocapData[userIdx];

		// name of marker set
		sprintf_s(pMarkerDesc->szName, sizeof(pMarkerDesc->szName), "User%d", userIdx + 1);
		strcpy_s(msData.szName, sizeof(msData.szName), pMarkerDesc->szName);

		// number of markers
		pMarkerDesc->nMarkers = SKELETON_DATA_COUNT;
		msData.nMarkers = pMarkerDesc->nMarkers;

		pMarkerDesc->szMarkerNames = new char*[pMarkerDesc->nMarkers];
		msData.Markers = new MarkerData[msData.nMarkers];

		for (int m = 0; m < pMarkerDesc->nMarkers; m++)
		{
			pMarkerDesc->szMarkerNames[m] = _strdup(SKELETON_DATA[m].czPositionName);
		}

		// add to description list
		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_MarkerSet;
		refData.description.arrDataDescriptions[descrIdx].Data.MarkerSetDescription = pMarkerDesc;
		descrIdx++;
	}

	for (int nRigid = 0; nRigid < MAX_USERS; nRigid++)
	{
		sSkeletonDescription* pSkeletonDesc = new sSkeletonDescription();
		sSkeletonData& skData = refData.frame.Skeletons[nRigid];

		sprintf_s(pSkeletonDesc->szName, sizeof(pSkeletonDesc->szName), "User%d", nRigid + 1);

		pSkeletonDesc->skeletonID = nRigid;
		skData.skeletonID = pSkeletonDesc->skeletonID;

		pSkeletonDesc->nRigidBodies = BONE_DATA_COUNT;
		skData.nRigidBodies = pSkeletonDesc->nRigidBodies;

		skData.RigidBodyData = new sRigidBodyData[pSkeletonDesc->nRigidBodies];

		for (int rbodies = 0; rbodies < pSkeletonDesc->nRigidBodies; rbodies++)
		{
			readRigidBodyDescription(pSkeletonDesc->RigidBodies[rbodies], skData.RigidBodyData[rbodies], rbodies);
		}

		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_Skeleton;
		refData.description.arrDataDescriptions[descrIdx].Data.SkeletonDescription = pSkeletonDesc;
		descrIdx++;
	}

	refData.description.nDataDescriptions = descrIdx;

	// pre-fill in frame data
	refData.frame.nMarkerSets = MAX_USERS;
	refData.frame.nSkeletons  = MAX_USERS;

	return true;
}


void MoCapKinect::readRigidBodyDescription(sRigidBodyDescription &descr, sRigidBodyData& data, int rbodies)
{
	descr.ID = rbodies;
	strcpy_s(descr.szName, sizeof(descr.szName), BONE_DATA[rbodies].czBoneName);
	descr.parentID = BONE_DATA[rbodies].parentIndex;
	descr.offsetx = 0; descr.offsety = 0; descr.offsetz = 0;

	data.ID          = descr.ID;
	data.nMarkers    = 0;
	data.Markers     = nullptr;
	data.MarkerIDs   = nullptr;
	data.MarkerSizes = nullptr;
	data.MeanError   = 0;
}


bool MoCapKinect::processCommand(const std::string& strCommand)
{
	// no commands implemented yet
	return false;
}


bool MoCapKinect::isRunning()
{
	return running;
}


void MoCapKinect::setRunning(bool running)
{
	this->running = running;
}


bool MoCapKinect::update()
{
	if (running)
	{
		signalNewFrame();
	}
	return true;
}


void MoCapKinect::handleSkeletonData(const NUI_SKELETON_FRAME& refSkeletonFrame, MoCapData& refData)
{
	// handle lost and found users
	checkUserLost(refSkeletonFrame);
	checkUserFound(refSkeletonFrame);

	// transfer data to MoCap data structure
	for (int userIdx = 0; userIdx < MAX_USERS; userIdx++)
	{
		if (userSkeletonIdx[userIdx] != USER_NOT_TRACKED)
		{
			// TODO: Do a proper transformation using the clip plane coefficients
			// e.g., https://gamedev.stackexchange.com/questions/80310/transform-world-space-using-kinect-floorclipplane-to-move-origin-to-floor-while
			float yOffset = refSkeletonFrame.vFloorClipPlane.w;

			const NUI_SKELETON_DATA& skeleton = refSkeletonFrame.SkeletonData[userSkeletonIdx[userIdx]];

			NUI_SKELETON_BONE_ORIENTATION boneOrientations[NUI_SKELETON_POSITION_COUNT];
			NuiSkeletonCalculateBoneOrientations(&skeleton, boneOrientations);

			sMarkerSetData& msData = refData.frame.MocapData[userIdx];
			// go through all bones
			for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
			{
				int            skeletonIdx = SKELETON_DATA[mIdx].index;
				const Vector4& point = skeleton.SkeletonPositions[skeletonIdx];
				MarkerData&    msMarker = msData.Markers[mIdx];

				switch (skeleton.eSkeletonPositionTrackingState[skeletonIdx])
				{
					case NUI_SKELETON_POSITION_INFERRED:
						//fallthrough
					case NUI_SKELETON_POSITION_TRACKED:
						msMarker[0] = point.x;
						msMarker[1] = point.y + yOffset; // TODO: see above re transformation
						msMarker[2] = point.z;
						break;

					default:
						// not tracked > zero out to indicate "invalid/untracked"
						msMarker[0] = 0;
						msMarker[1] = 0;
						msMarker[2] = 0;
						break;
				}
			}

			sSkeletonData& skeleData = refData.frame.Skeletons[userIdx];
			if (skeleton.eTrackingState == NUI_SKELETON_TRACKED)
			{
				for (int j = 0; j < BONE_DATA_COUNT; j++)
				{
					NUI_SKELETON_BONE_ORIENTATION& orientation = boneOrientations[SKELETON_DATA[j].boneIndex];
					sRigidBodyData&    rigidData = skeleData.RigidBodyData[j];
					if (j == 0) 
					{
						const Vector4& point = skeleton.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];
						rigidData.x = point.x;
						rigidData.y = point.y + yOffset; // TODO: see above re transformation
						rigidData.z = point.z;
					}
					else 
					{
						rigidData.x = 0;
						rigidData.y = findDy(SKELETON_DATA[j].index, skeleton);
						rigidData.z = 0;
					}
					rigidData.qw = orientation.hierarchicalRotation.rotationQuaternion.w;
					rigidData.qx = orientation.hierarchicalRotation.rotationQuaternion.x;
					rigidData.qy = orientation.hierarchicalRotation.rotationQuaternion.y;
					rigidData.qz = orientation.hierarchicalRotation.rotationQuaternion.z;
					rigidData.MeanError = findBoneLength(SKELETON_DATA[j].index, skeleton);
					rigidData.params = STATUS_TRACKED;
				}
			}
			else
			{
				// not tracked: reset skeleton data
				refData.resetSkeletonData(skeleData);
			}
		}
		else
		{
			// user not tracked at all > zero out all the data
			sMarkerSetData& msData = refData.frame.MocapData[userIdx];
			refData.resetMarkerData(msData);
			sSkeletonData& skeleData = refData.frame.Skeletons[userIdx];
			refData.resetSkeletonData(skeleData);
		}
	}
}

//Find RigidData.y of the bone
float MoCapKinect::findDy(int aindex, NUI_SKELETON_DATA skeleton)
{
	const Vector4& endPoint = skeleton.SkeletonPositions[BONE_DATA[aindex].kPoint];
	const Vector4& startPoint = skeleton.SkeletonPositions[BONE_DATA[aindex].kParent];
	float e = 0;
	float x = endPoint.x - startPoint.x;
	float y = endPoint.y - startPoint.y;
	float z = endPoint.z - startPoint.z;

	x = x * x;
	y = y * y;
	z = z * z;

	e = sqrt(x + y + z);

	return e;
}

//Find the length of the bone
float MoCapKinect::findBoneLength(int aindex, NUI_SKELETON_DATA skeleton)
{
	const Vector4& endPoint = skeleton.SkeletonPositions[BONE_DATA[aindex].kEnd];
	const Vector4& startPoint = skeleton.SkeletonPositions[BONE_DATA[aindex].kPoint];
	float e = 0;
	float x = endPoint.x - startPoint.x;
	float y = endPoint.y - startPoint.y;
	float z = endPoint.z - startPoint.z;

	x = x * x;
	y = y * y;
	z = z * z;

	e = sqrt(x + y + z);

	return e;
}


void MoCapKinect::checkUserLost(const NUI_SKELETON_FRAME& refSkeletonFrame)
{
	for (int userIdx = 0; userIdx < MAX_USERS; userIdx++)
	{
		if (userSkeletonIdx[userIdx] != USER_NOT_TRACKED)
		{
			const NUI_SKELETON_DATA& skeleton = refSkeletonFrame.SkeletonData[userSkeletonIdx[userIdx]];
			if (skeleton.eTrackingState == NUI_SKELETON_NOT_TRACKED)
			{
				userSkeletonIdx[userIdx] = USER_NOT_TRACKED;
				LOG_INFO("Lost user " << userIdx);
			}
		}
	}
}


void MoCapKinect::checkUserFound(const NUI_SKELETON_FRAME& refSkeletonFrame)
{
	for (int skeletonIdx = 0; skeletonIdx < NUI_SKELETON_COUNT; skeletonIdx++)
	{
		const NUI_SKELETON_DATA& skeleton = refSkeletonFrame.SkeletonData[skeletonIdx];
		// only check for fully tracked users, not POSITION_ONLY
		if (skeleton.eTrackingState == NUI_SKELETON_TRACKED)
		{
			// TODO: Hardcoded for a maximum of two users. What if there are more possible...?
			if (userSkeletonIdx[0] < 0 && skeletonIdx != userSkeletonIdx[1])
			{
				userSkeletonIdx[0] = skeletonIdx;
				LOG_INFO("Found user 0 (skeleton Idx " << skeletonIdx << ")");
			}
			else if (userSkeletonIdx[1] < 0 && skeletonIdx != userSkeletonIdx[0])
			{
				userSkeletonIdx[1] = skeletonIdx;
				LOG_INFO("Found user 1 (skeleton Idx " << skeletonIdx << ")");
			}
		}
	}
}


bool MoCapKinect::deinitialise()
{
	if (initialised)
	{
		// avoid causing another blocking frame read during deinitialisation phase
		running = false;

		cleanup();
		initialised = false;
	}
	return !initialised;
}


void MoCapKinect::cleanup()
{
	if (pNuiSensor != nullptr)
	{
		pNuiSensor->Release();
		pNuiSensor = nullptr;
	}

	NuiShutdown();
}


MoCapKinect::~MoCapKinect()
{
	deinitialise();
}


#endif