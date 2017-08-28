#include "MocapKinect.h"

#ifdef USE_KINECT

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapKinect"

#include "VectorMath.h"

#include <algorithm>
#include <iterator>
#include <string>


#define MAX_USERS         NUI_SKELETON_MAX_TRACKED_COUNT
#define USER_NOT_TRACKED -1


/**
 * Structure for associating marker names with their Kinect skeleton ID
 */
struct sKinectSkeletonData 
{
	const char*                       czPositionName;
	const NUI_SKELETON_POSITION_INDEX index;
	const int boneIndex;
	const int parentIndex;
};


const sKinectSkeletonData SKELETON_DATA[]
{
	{ "HipCentre",     NUI_SKELETON_POSITION_HIP_CENTER, 0,-1},
	{ "Spine",         NUI_SKELETON_POSITION_SPINE, 1, 0},
	{ "HipLeft",       NUI_SKELETON_POSITION_HIP_LEFT, 2, 0},
	{ "HipRight",      NUI_SKELETON_POSITION_HIP_RIGHT, 3, 0},
	{ "ShoulderCentre",NUI_SKELETON_POSITION_SHOULDER_CENTER, 4, 1},
	{ "KneeLeft",      NUI_SKELETON_POSITION_KNEE_LEFT, 5, 2},
	{ "KneeRight",     NUI_SKELETON_POSITION_KNEE_RIGHT, 6, 3},
	{ "ShoulderLeft",  NUI_SKELETON_POSITION_SHOULDER_LEFT, 7, 4},
	{ "Head",          NUI_SKELETON_POSITION_HEAD, 8, 4},
	{ "ShoulderRight", NUI_SKELETON_POSITION_SHOULDER_RIGHT, 9, 4},
	{ "AnkleLeft",     NUI_SKELETON_POSITION_ANKLE_LEFT, 10, 5},
	{ "AnkleRight",    NUI_SKELETON_POSITION_ANKLE_RIGHT, 11, 6},
	{ "ElbowLeft",     NUI_SKELETON_POSITION_ELBOW_LEFT, 12, 7},
	{ "ElbowRight",    NUI_SKELETON_POSITION_ELBOW_RIGHT, 13, 9},
	{ "FootLeft",      NUI_SKELETON_POSITION_FOOT_LEFT, 14, 10},
	{ "FootRight",     NUI_SKELETON_POSITION_FOOT_RIGHT, 15, 11},
	{ "WristLeft",     NUI_SKELETON_POSITION_WRIST_LEFT, 16, 12},
	{ "WristRight",    NUI_SKELETON_POSITION_WRIST_RIGHT, 17, 13},
	{ "HandLeft",      NUI_SKELETON_POSITION_HAND_LEFT, 18, 16},
	{ "HandRight",     NUI_SKELETON_POSITION_HAND_RIGHT, 19, 17},
};

const int SKELETON_DATA_COUNT = sizeof(SKELETON_DATA) / sizeof(SKELETON_DATA[0]);



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
	pNuiSensor(NULL)
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
			kinectHandle = CreateEventW(NULL, TRUE, FALSE, NULL);
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
			refData.frame.iFrame     = (int) skeletonFrame.dwFrameNumber;
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

	for (int nRigid = 0; nRigid < MAX_USERS; nRigid++){
		sSkeletonDescription* pSkeletonDesc = new sSkeletonDescription();
		sSkeletonData& skData = refData.frame.Skeletons[nRigid];

		sprintf_s(pSkeletonDesc->szName, sizeof(pSkeletonDesc->szName), "User%d Rigid", nRigid + 1);
		
		pSkeletonDesc->skeletonID = nRigid;
		skData.skeletonID = pSkeletonDesc->skeletonID;

		pSkeletonDesc->nRigidBodies = SKELETON_DATA_COUNT;
		skData.nRigidBodies = pSkeletonDesc->nRigidBodies;

		skData.RigidBodyData = new sRigidBodyData[pSkeletonDesc->nRigidBodies];

		for (int rbodies = 0; rbodies < pSkeletonDesc->nRigidBodies; rbodies++) {
			readRigidBodyDescription(pSkeletonDesc->RigidBodies[rbodies], skData.RigidBodyData[rbodies], rbodies);
		}

		refData.description.arrDataDescriptions[descrIdx].type = Descriptor_Skeleton;
		refData.description.arrDataDescriptions[descrIdx].Data.SkeletonDescription = pSkeletonDesc;
		descrIdx++;
	}


	refData.description.nDataDescriptions = descrIdx;

	// pre-fill in frame data
	refData.frame.nMarkerSets = MAX_USERS;
	refData.frame.nSkeletons = MAX_USERS;

	return true;
}

void MoCapKinect::readRigidBodyDescription(sRigidBodyDescription &descr, sRigidBodyData& data, int rbodies)
{
	descr.ID = rbodies;
	strcpy_s(descr.szName, sizeof(descr.szName), SKELETON_DATA[rbodies].czPositionName);
	descr.parentID = SKELETON_DATA[rbodies].parentIndex;
	descr.offsetx = 0; descr.offsety = 0; descr.offsetz = 0;

	data.ID = descr.ID;
	data.nMarkers = 0;
	data.Markers = NULL;
	data.MarkerIDs = NULL;
	data.MarkerSizes = NULL;
	data.MeanError = 1;
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

			sMarkerSetData&          msData   = refData.frame.MocapData[userIdx];
			sSkeletonData&          skeleData = refData.frame.Skeletons[userIdx];

			//LOG_INFO_START("Kinect skeleton " << firstUser <<
			//	"\tx:" << skeleton.Position.x << "\ty:" << skeleton.Position.y << "\tz:" << skeleton.Position.z << "\t");

			// go through all bones
			for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
			{
				int            skeletonIdx = SKELETON_DATA[mIdx].index;
				const Vector4& point       = skeleton.SkeletonPositions[skeletonIdx];
				MarkerData&    msMarker    = msData.Markers[mIdx];

				//LOG_INFO_MID(skeleton.eSkeletonPositionTrackingState[mIdx]);

				switch (skeleton.eSkeletonPositionTrackingState[skeletonIdx])
				{
				case NUI_SKELETON_POSITION_INFERRED:
					//fallthrough
				case NUI_SKELETON_POSITION_TRACKED:
					msMarker[0] = point.x;
					msMarker[1] = point.y + yOffset; // TODO: see above re transformation
					msMarker[2] = point.z;
					skeleData.RigidBodyData[mIdx].x = point.x;
					skeleData.RigidBodyData[mIdx].y = point.y + yOffset;
					skeleData.RigidBodyData[mIdx].z = point.z;
					break;

				default:
					// not tracked > zero out to indicate "invalid/untracked"
					msMarker[0] = 0;
					msMarker[1] = 0;
					msMarker[2] = 0;
					break;
				}
			}

			if (skeleton.eTrackingState == NUI_SKELETON_TRACKED) {
				for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; j++) {
						NUI_SKELETON_BONE_ORIENTATION & orientation = boneOrientations[j];
						sRigidBodyData&    rigidData = skeleData.RigidBodyData[j];

						rigidData.qw = orientation.hierarchicalRotation.rotationQuaternion.w;
						rigidData.qx = orientation.hierarchicalRotation.rotationQuaternion.x;
						rigidData.qy = orientation.hierarchicalRotation.rotationQuaternion.y;
						rigidData.qz = orientation.hierarchicalRotation.rotationQuaternion.z;
						skeleData.RigidBodyData[j].params = 0x01;

				}
			}
			//LOG_INFO_END();
		}
		else
		{
			// user not tracked at all > zero out all the data
			sMarkerSetData& msData = refData.frame.MocapData[userIdx];

			for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
			{
				MarkerData& msMarker = msData.Markers[mIdx];
				msMarker[0] = 0;
				msMarker[1] = 0;
				msMarker[2] = 0;
			}

			sSkeletonData& skeleData = refData.frame.Skeletons[userIdx];

			for (int rIdx = 0; rIdx < skeleData.nRigidBodies; rIdx++)
			{
				skeleData.RigidBodyData[rIdx].qw = 1;
				skeleData.RigidBodyData[rIdx].qx = 0;
				skeleData.RigidBodyData[rIdx].qy = 0;
				skeleData.RigidBodyData[rIdx].qz = 0;
				skeleData.RigidBodyData[rIdx].x = 0;
				skeleData.RigidBodyData[rIdx].y = 0;
				skeleData.RigidBodyData[rIdx].z = 0;
				skeleData.RigidBodyData[rIdx].params = 0x00;

			}
		}
	}
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
			}
			else if (userSkeletonIdx[1] < 0 && skeletonIdx != userSkeletonIdx[0])
			{
				userSkeletonIdx[1] = skeletonIdx;
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
	if (pNuiSensor != NULL)
	{
		pNuiSensor->Release();
		pNuiSensor = NULL;
	}

	NuiShutdown();
}


MoCapKinect::~MoCapKinect()
{
	deinitialise();
}


#endif
