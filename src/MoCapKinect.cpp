#include "MocapKinect.h"

#ifdef USE_KINECT

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapKinect"

#include "VectorMath.h"

#include <algorithm>
#include <iterator>
#include <string>


// Structure for associating marker names with their Kinect skeleton ID
struct sKinectSkeletonData 
{
	const char*                       czPositionName;
	const NUI_SKELETON_POSITION_INDEX index;
};

const sKinectSkeletonData SKELETON_DATA[]
{
	{ "HipCentre",     NUI_SKELETON_POSITION_HIP_CENTER },
	{ "HipRight",      NUI_SKELETON_POSITION_HIP_RIGHT },
	{ "KneeRight",     NUI_SKELETON_POSITION_KNEE_RIGHT },
	{ "AnkleRight",    NUI_SKELETON_POSITION_ANKLE_RIGHT },
	{ "FootRight",     NUI_SKELETON_POSITION_FOOT_RIGHT },
	{ "HipLeft",       NUI_SKELETON_POSITION_HIP_LEFT },
	{ "KneeLeft",      NUI_SKELETON_POSITION_KNEE_LEFT },
	{ "AnkleLeft",     NUI_SKELETON_POSITION_ANKLE_LEFT },
	{ "FootLeft",      NUI_SKELETON_POSITION_FOOT_LEFT },
	{ "Spine",         NUI_SKELETON_POSITION_SPINE },
	{ "ShoulderCentre",NUI_SKELETON_POSITION_SHOULDER_CENTER },
	{ "Head",          NUI_SKELETON_POSITION_HEAD },
	{ "ShoulderRight", NUI_SKELETON_POSITION_SHOULDER_RIGHT },
	{ "ElbowRight",    NUI_SKELETON_POSITION_ELBOW_RIGHT },
	{ "WristRight",    NUI_SKELETON_POSITION_WRIST_RIGHT },
	{ "HandRight",     NUI_SKELETON_POSITION_HAND_RIGHT },
	{ "ShoulderLeft",  NUI_SKELETON_POSITION_SHOULDER_LEFT },
	{ "ElbowLeft",     NUI_SKELETON_POSITION_ELBOW_LEFT },
	{ "WristLeft",     NUI_SKELETON_POSITION_WRIST_LEFT },
	{ "HandLeft",      NUI_SKELETON_POSITION_HAND_LEFT },
};

const int SKELETON_DATA_COUNT = sizeof(SKELETON_DATA) / sizeof(SKELETON_DATA[0]);



MoCapKinectConfiguration::MoCapKinectConfiguration() :
	SystemConfiguration("Kinect"),
	useKinect(false),
	seatedMode(false)
{
	addOption("-useKinect",  "Search for and use a Kinect sensor if connected");
	addOption("-seatedMode", "Do not track the legs and feet");
}


bool MoCapKinectConfiguration::handleParameter(int idx, const std::string& value)
{
	bool success = true;
	switch (idx)
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



MoCapKinect::MoCapKinect(MoCapKinectConfiguration configuration) :
	configuration(configuration),
	initialised(false),
	running(true)
{
	// nothing else to do
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
			LOG_INFO("Kinect sensor initialized" << (configuration.seatedMode ? "(seated mode)" : ""));
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
	// TODO: Handle more than one user in an organised way
	// This code below will simply search for the first tracked skeleton
	int firstUser = -1;
	for (int userIdx = 0; userIdx < NUI_SKELETON_COUNT; userIdx++)
	{
		const NUI_SKELETON_DATA& skeleton = refSkeletonFrame.SkeletonData[userIdx];
		// only check for fully tracked users, not POSITION_ONLY
		if (skeleton.eTrackingState == NUI_SKELETON_TRACKED)
		{
			firstUser = userIdx;
			break;
		}
	}

	if (firstUser >= 0)
	{
		// TODO: Do a proper transformation using the clip plane coefficients
		// e.g., https://gamedev.stackexchange.com/questions/80310/transform-world-space-using-kinect-floorclipplane-to-move-origin-to-floor-while
		float yOffset = refSkeletonFrame.vFloorClipPlane.w;

		const NUI_SKELETON_DATA& skeleton = refSkeletonFrame.SkeletonData[firstUser];
		sMarkerSetData&          msData   = refData.frame.MocapData[0];
		
		//LOG_INFO_START("Kinect skeleton " << firstUser <<
		//	"\tx:" << skeleton.Position.x << "\ty:" << skeleton.Position.y << "\tz:" << skeleton.Position.z << "\t");

		// go through all bones
		for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
		{
			const Vector4& point    = skeleton.SkeletonPositions[SKELETON_DATA[mIdx].index];
			MarkerData&    msMarker = msData.Markers[mIdx];

			//LOG_INFO_MID(skeleton.eSkeletonPositionTrackingState[mIdx]);

			switch (skeleton.eSkeletonPositionTrackingState[mIdx])
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
		//LOG_INFO_END();
	}
	else
	{
		// nothing tracked > zero out all the data
		sMarkerSetData& msData = refData.frame.MocapData[0];

		for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
		{
			MarkerData& msMarker = msData.Markers[mIdx];
			msMarker[0] = 0;
			msMarker[1] = 0;
			msMarker[2] = 0;
		}
	}
}


bool MoCapKinect::getSceneDescription(MoCapData& refData)
{
	int descrIdx = 0;

	// create markerset description and frame
	sMarkerSetDescription* pMarkerDesc = new sMarkerSetDescription();
	sMarkerSetData&        msData      = refData.frame.MocapData[0];

	// name of marker set
	strcpy_s(pMarkerDesc->szName, sizeof(pMarkerDesc->szName), "User1");
	strcpy_s(msData.szName, sizeof(msData.szName), pMarkerDesc->szName);

	// number of markers
	pMarkerDesc->nMarkers = SKELETON_DATA_COUNT;
	msData.nMarkers       = SKELETON_DATA_COUNT;

	pMarkerDesc->szMarkerNames = new char*[pMarkerDesc->nMarkers];
	msData.Markers             = new MarkerData[msData.nMarkers];

	for (int m = 0; m < pMarkerDesc->nMarkers; m++)
	{
		pMarkerDesc->szMarkerNames[m] = _strdup(SKELETON_DATA[m].czPositionName);
	}

	// add to description list
	refData.description.arrDataDescriptions[descrIdx].type = Descriptor_MarkerSet;
	refData.description.arrDataDescriptions[descrIdx].Data.MarkerSetDescription = pMarkerDesc;
	descrIdx++;


	refData.description.nDataDescriptions = descrIdx;

	// pre-fill in frame data
	refData.frame.nMarkerSets  = 1;
	refData.frame.nRigidBodies = 0;
	refData.frame.nSkeletons   = 0;

	refData.frame.nOtherMarkers = 0;
	refData.frame.OtherMarkers  = NULL;

	refData.frame.nLabeledMarkers = 0;

	refData.frame.nForcePlates = 0;

	refData.frame.fLatency = 0;
	refData.frame.Timecode = 0;
	refData.frame.TimecodeSubframe = 0;

	return true;
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
	NuiShutdown();
}


MoCapKinect::~MoCapKinect()
{
	deinitialise();
}


#endif
