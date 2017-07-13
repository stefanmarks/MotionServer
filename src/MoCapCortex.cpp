#include "MoCapCortex.h"

#ifdef USE_CORTEX

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapCortex"

#include "VectorMath.h"

#include <algorithm>
#include <iterator>
#include <string>


#define MAX_UNKNOWN_MARKERS 256 // maximum number of unknown markers



MoCapCortexConfiguration::MoCapCortexConfiguration() :
	SystemConfiguration("Cortex"),
	useCortex(false),
	remoteCortexAddress(""),
	localCortexAddress("")
{
	addParameter("-cortexRemoteAddr", "<address>", "IP Address of remote interface to connect to Cortex");
	addParameter("-cortexLocalAddr", "<address>",  "IP Address of local interface to connect to Cortex");
}


bool MoCapCortexConfiguration::handleParameter(int idx, const std::string& value)
{
	bool success = true;
	switch (idx)
	{
		case 0:
			remoteCortexAddress = value;
			useCortex = true;
			break;

		case 1:
			localCortexAddress = value;

		default:
			success = false;
			break;
	}
	return success;
}




/**
 * Handler for messages from the Cortex server.
 */
void __cdecl callbackMoCapCortexMessageHandler(int iMessageType, char* czMessage)
{
	switch (iMessageType)
	{
	case VL_Error:
	case VL_Warning:
		LOG_ERROR(czMessage);
		break;
	default:
		LOG_INFO(czMessage);
		break;
	}
}


/**
 * Handler for frames from the Cortex server.
 */
void __cdecl callbackMoCapCortexDataHandler(sFrameOfData* pFrameOfData)
{
	// process data
	// merely signal new frame, the main system will pick up the data through getFrameData(...)
	// which uses Cortex_GetCurrentFrame() 
	// which hopefully will have this frame data
	signalNewFrame();
}


MoCapCortex::MoCapCortex(MoCapCortexConfiguration configuration) :
	configuration(configuration),
	initialised(false),
	running(true),
	pCortexInfo(NULL),
	unitScaleFactor(1.0f),
	updateRate(100.0f),
	handleUnknownMarkers(false)
{
	// nothing else to do
}


bool MoCapCortex::initialise()
{
	if (!initialised)
	{
		// print version info
		unsigned char cortexSDK_Version[4];
		Cortex_GetSdkVersion(cortexSDK_Version);
		LOG_INFO("Cortex SDK version v"
			<< (int)cortexSDK_Version[1] << "."
			<< (int)cortexSDK_Version[2] << "."
			<< (int)cortexSDK_Version[3]);

		LOG_INFO("Connecting to Cortex server at address " << configuration.remoteCortexAddress
			<< (configuration.localCortexAddress.empty() ? "" : " from address ") << configuration.localCortexAddress);

		// set up callback handler for logging and streaming
		Cortex_SetErrorMsgHandlerFunc(callbackMoCapCortexMessageHandler);
		Cortex_SetDataHandlerFunc(callbackMoCapCortexDataHandler);

		if (Cortex_Initialize(
				(char*) (configuration.localCortexAddress.empty() ? NULL : configuration.localCortexAddress.c_str()),
				(char*)  configuration.remoteCortexAddress.c_str()
			) == RC_Okay)
		{
			pCortexInfo = new sHostInfo;
			if ((Cortex_GetHostInfo(pCortexInfo) == RC_Okay) && pCortexInfo->bFoundHost)
			{
				LOG_INFO("Connected to Cortex server "
					<< pCortexInfo->szHostProgramName << " v"
					<< (int)pCortexInfo->HostProgramVersion[1] << "."
					<< (int)pCortexInfo->HostProgramVersion[2] << "."
					<< (int)pCortexInfo->HostProgramVersion[3] << " at address "
					<< (int)pCortexInfo->HostMachineAddress[0] << "."
					<< (int)pCortexInfo->HostMachineAddress[1] << "."
					<< (int)pCortexInfo->HostMachineAddress[2] << "."
					<< (int)pCortexInfo->HostMachineAddress[3] << " ("
					<< pCortexInfo->szHostMachineName << ")");

				int portHost, portHostMulticast;
				Cortex_GetPortNumbers(
					NULL, //int* TalkToHostPort,
					&portHost,
					&portHostMulticast,
					NULL, //int *TalkToClientsRequestPort,
					NULL, //int *TalkToClientsMulticastPort,
					NULL  //int *ClientsMulticastPort);
				);
				LOG_INFO("Host port: "           << portHost);
				LOG_INFO("Host Multicast port: " << portHostMulticast);

				// determine unit conversion factor
				float unitToMillimeter = 1;
				void *pResponse = NULL;
				int  iResponseSize = 0;
				if (Cortex_Request("GetConversionToMillimeters", &pResponse, &iResponseSize) == RC_Okay)
				{
					unitToMillimeter = *((float*)pResponse);
					LOG_INFO("Units to millimeters: " << unitToMillimeter);
				}
				unitScaleFactor = unitToMillimeter / 1000.0f; // convert millimeters to units

				// determine update rate
				updateRate = 100.0f; // default usually around 100
				if (Cortex_Request("GetContextFrameRate", &pResponse, &iResponseSize) == RC_Okay)
				{
					updateRate = *((float*)pResponse);
					LOG_INFO("Cortex Framerate: " << updateRate);
				}

				LOG_INFO("Initialised");

				initialised = true;
			}
			else
			{
				LOG_ERROR("Could not communicate with Cortex server");
				delete pCortexInfo;
				pCortexInfo = NULL;
				Cortex_Exit();
			}
		}
	}
	return initialised;
}


bool MoCapCortex::isActive()
{
	return initialised;
}


float MoCapCortex::getUpdateRate()
{
	return updateRate;
}


bool MoCapCortex::isRunning()
{
	return running;
}


void MoCapCortex::setRunning(bool running)
{
	void  *pResponse = NULL;
	int   iResponseSize = 0;
	char* czCommand = running ? "LiveMode" : "Pause";
	if (Cortex_Request(czCommand, &pResponse, &iResponseSize) == RC_Okay)
	{
		this->running = running;
	}
}


bool MoCapCortex::update()
{
	// nothing to do here > Cortex callback function works in the background
	return true;
}


bool MoCapCortex::getSceneDescription(MoCapData& refData)
{
	bool success = false;
	if (initialised)
	{
		LOG_INFO("Requesting scene description")
		sBodyDefs* pBodyDefs = Cortex_GetBodyDefs();

		if (pBodyDefs != NULL)
		{
			convertCortexDescriptionToNatNet(*pBodyDefs, refData.description, refData.frame);
			Cortex_FreeBodyDefs(pBodyDefs);
			success = true;
		}
		else
		{
			LOG_ERROR("Could not retrieve scene information from Cortex");
		}
	}

	return success;
}


bool MoCapCortex::getFrameData(MoCapData& refData)
{
	bool success = false;

	if (initialised)
	{
		// only request data from Cortex when not getting it via callback
		sFrameOfData* pFrame = Cortex_GetCurrentFrame();

		if (pFrame != NULL)
		{
			if (!convertCortexFrameToNatNet(*pFrame, refData.frame))
			{
				// conversion failed - scene was updated?
				getSceneDescription(refData);
				// now try converting the frame again
				convertCortexFrameToNatNet(*pFrame, refData.frame);
			}
			Cortex_FreeFrame(pFrame);
			success = true;
		}
		else
		{
			LOG_ERROR("Could not retrieve frame data from Cortex");
		}
	}

	return success;
}


bool MoCapCortex::isHandlingUnknownMarkers()
{
	return handleUnknownMarkers;
}


void MoCapCortex::setHandleUnknownMarkers(bool enable)
{
	handleUnknownMarkers = enable;
	LOG_INFO("Unknown markers: " << (handleUnknownMarkers ? "enabled" : "disabled"));
}


bool MoCapCortex::processCommand(const std::string& strCommand)
{
	bool processed = false;

	// convert command to lowercase
	std::string strCmdLowerCase;
	std::transform(strCommand.begin(), strCommand.end(), std::back_inserter(strCmdLowerCase), ::tolower);

	if (strCmdLowerCase == "enableunknownmarkers")
	{
		setHandleUnknownMarkers(true);
		processed = true;
	}
	else if (strCmdLowerCase == "disableunknownmarkers")
	{
		setHandleUnknownMarkers(false);
		processed = true;
	}

	return processed;
}


bool MoCapCortex::deinitialise()
{
	if (initialised)
	{
		Cortex_SetDataHandlerFunc(NULL);

		delete pCortexInfo;
		pCortexInfo = NULL;
		Cortex_Exit();

		Cortex_SetErrorMsgHandlerFunc(NULL);

		LOG_INFO("Deinitialised");

		initialised = false;
	}
	return !initialised;
}


MoCapCortex::~MoCapCortex()
{
	deinitialise();
}


void MoCapCortex::convertCortexDescriptionToNatNet(sBodyDefs& refCortex, sDataDescriptions& refDescr, sFrameOfMocapData& refFrame)
{
	int idxDataBlock = 0;
	int idxMarkerSet = 0;
	int idxRigidBody = 0;
	int idxSkeleton = 0;

	for (int iBodyIdx = 0; iBodyIdx < refCortex.nBodyDefs; iBodyIdx++)
	{
		sBodyDef& bodyDef = refCortex.BodyDefs[iBodyIdx];

		// create markerset description and markerset data
		sMarkerSetDescription* pMarkerSetDescr = new sMarkerSetDescription;
		sMarkerSetData&        refMarkerSetData = refFrame.MocapData[idxMarkerSet];

		// markerset name
		strncpy_s(pMarkerSetDescr->szName, bodyDef.szName, sizeof(pMarkerSetDescr->szName));
		strncpy_s(refMarkerSetData.szName, bodyDef.szName, sizeof(refMarkerSetData.szName));

		// number of markers
		int nMarkers = bodyDef.nMarkers;
		pMarkerSetDescr->nMarkers = nMarkers;
		refMarkerSetData.nMarkers = nMarkers;

		// array of marker names
		pMarkerSetDescr->szMarkerNames = new char*[nMarkers];
		for (int mIdx = 0; mIdx < bodyDef.nMarkers; mIdx++)
		{
			pMarkerSetDescr->szMarkerNames[mIdx] = _strdup(bodyDef.szMarkerNames[mIdx]);
		}

		// array of marker data
		refMarkerSetData.Markers = new MarkerData[nMarkers];

		// add to description block
		refDescr.arrDataDescriptions[idxDataBlock].type = Descriptor_MarkerSet;
		refDescr.arrDataDescriptions[idxDataBlock].Data.MarkerSetDescription = pMarkerSetDescr;
		idxDataBlock++;
		idxMarkerSet++;

		sHierarchy& refSkeleton = bodyDef.Hierarchy;
		if (refSkeleton.nSegments == 1)
		{
			// one bone skeleton -> treat as rigid body
			// create rigid body description
			sRigidBodyDescription* pRigidBodyDescr = new sRigidBodyDescription;
			strncpy_s(pRigidBodyDescr->szName, bodyDef.szName, sizeof(pRigidBodyDescr->szName)); // rigid body name
			pRigidBodyDescr->ID = iBodyIdx; // rigid body ID = actor ID
			pRigidBodyDescr->parentID = -1; // no parent
			pRigidBodyDescr->offsetx = 0;   // the offset does not exist in Cortex data
			pRigidBodyDescr->offsety = 0;
			pRigidBodyDescr->offsetz = 0;

			// pre-fill rigid body frame data structure
			sRigidBodyData& refRigidBodyData = refFrame.RigidBodies[idxRigidBody];
			refRigidBodyData.ID = iBodyIdx;
			refRigidBodyData.nMarkers = 0;
			refRigidBodyData.Markers = NULL;
			refRigidBodyData.MarkerIDs = NULL;
			refRigidBodyData.MarkerSizes = NULL;
			refRigidBodyData.MeanError = 0;

			// add to scene description
			refDescr.arrDataDescriptions[idxDataBlock].type = Descriptor_RigidBody;
			refDescr.arrDataDescriptions[idxDataBlock].Data.RigidBodyDescription = pRigidBodyDescr;
			idxDataBlock++;
			idxRigidBody++;
		}
		else if (refSkeleton.nSegments > 0)
		{
			// skeleton data included as well
			// create skeleton description and skeleton data
			sSkeletonDescription* pSkeletonDescr = new sSkeletonDescription;
			sSkeletonData&        refSkeletonData = refFrame.Skeletons[idxSkeleton];
			strncpy_s(pSkeletonDescr->szName, bodyDef.szName, sizeof(pSkeletonDescr->szName)); // markerset name = skeleton name
			pSkeletonDescr->skeletonID = iBodyIdx; // skeleton ID
			refSkeletonData.skeletonID = iBodyIdx;
			int nSegments = refSkeleton.nSegments;
			pSkeletonDescr->nRigidBodies = nSegments; // number of segments
			refSkeletonData.nRigidBodies = nSegments;
			refSkeletonData.RigidBodyData = new sRigidBodyData[nSegments];  // array of skeleton data
			for (int sIdx = 0; sIdx < nSegments; sIdx++)
			{
				// create skeleton segment description
				sRigidBodyDescription& refRigidBodyDescr = pSkeletonDescr->RigidBodies[sIdx];
				strncpy_s(refRigidBodyDescr.szName, refSkeleton.szSegmentNames[sIdx], sizeof(refRigidBodyDescr.szName)); // segment name
				refRigidBodyDescr.ID = sIdx; // segment ID
				refRigidBodyDescr.parentID = refSkeleton.iParents[sIdx]; // segment parent
				refRigidBodyDescr.offsetx = 0;   // the offset does not exist in Cortex data
				refRigidBodyDescr.offsety = 0;
				refRigidBodyDescr.offsetz = 0;

				// pre-fill skeleton segment frame data structure
				sRigidBodyData&  refRigidBodyData = refSkeletonData.RigidBodyData[sIdx];
				refRigidBodyData.ID = sIdx;
				refRigidBodyData.nMarkers = 0;
				refRigidBodyData.Markers = NULL;
				refRigidBodyData.MarkerIDs = NULL;
				refRigidBodyData.MarkerSizes = NULL;
				refRigidBodyData.MeanError = 0;
			}
			// add to description
			refDescr.arrDataDescriptions[idxDataBlock].type = Descriptor_Skeleton;
			refDescr.arrDataDescriptions[idxDataBlock].Data.SkeletonDescription = pSkeletonDescr;
			idxDataBlock++;
			idxSkeleton++;
		}
	}
	// store amount of data blocks
	refDescr.nDataDescriptions = idxDataBlock;

	// prepare amount of items in frame data
	refFrame.nMarkerSets = idxMarkerSet;
	refFrame.nOtherMarkers = 0;
	refFrame.OtherMarkers = new MarkerData[MAX_UNKNOWN_MARKERS];
	refFrame.nRigidBodies = idxRigidBody;
	refFrame.nSkeletons = idxSkeleton;
	refFrame.nLabeledMarkers = 0;
	refFrame.nForcePlates = 0;
	refFrame.Timecode = 0;
	refFrame.TimecodeSubframe = 0;
}


bool MoCapCortex::convertCortexFrameToNatNet(sFrameOfData& refCortex, sFrameOfMocapData& refNatNet)
{
	refNatNet.iFrame = refCortex.iFrame;
	refNatNet.fLatency = refCortex.fDelay;

	if (refCortex.nBodies != refNatNet.nMarkerSets)
	{
		LOG_ERROR("Mismatch in actor count");
		return false;
	}

	// copy marker data per actor
	for (int mIdx = 0; mIdx < refCortex.nBodies; mIdx++)
	{
		convertCortexMarkerSetToNatNet(refCortex.BodyData[mIdx], refNatNet.MocapData[mIdx]);
	}

	// copy rigid body data
	for (int rIdx = 0; rIdx < refNatNet.nRigidBodies; rIdx++)
	{
		int sourceIdx = refNatNet.RigidBodies[rIdx].ID;
		convertCortexSegmentToNatNet(refCortex.BodyData[sourceIdx].Segments[0], refNatNet.RigidBodies[rIdx]);
	}

	if (handleUnknownMarkers)
	{
		// copy unidentified marker data
		refNatNet.nOtherMarkers = refCortex.nUnidentifiedMarkers;
		for (int mIdx = 0; mIdx < refNatNet.nOtherMarkers; mIdx++)
		{
			convertCortexMarkerToNatNet(refCortex.UnidentifiedMarkers[mIdx], refNatNet.OtherMarkers[mIdx]);
		}
	}
	else
	{
		refNatNet.OtherMarkers = 0;
	}

	// copy skeleton data
	for (int sIdx = 0; sIdx < refNatNet.nSkeletons; sIdx++)
	{
		int sourceIdx = refNatNet.Skeletons[sIdx].skeletonID;
		convertCortexSegmentsToNatNet(refCortex.BodyData[sourceIdx], refNatNet.Skeletons[sIdx]);
	}

	return true;
}


void MoCapCortex::convertCortexMarkerToNatNet(tMarkerData& refCortex, MarkerData& refNatNet)
{
	if (refCortex[0] < XEMPTY)
	{
		refNatNet[0] = refCortex[0] * unitScaleFactor; // X
		refNatNet[1] = refCortex[1] * unitScaleFactor; // Y 
		refNatNet[2] = refCortex[2] * unitScaleFactor; // Z
	}
	else
	{
		// marker has vanished -> set position to exactly 0
		refNatNet[0] = 0;
		refNatNet[1] = 0;
		refNatNet[2] = 0;
	}
}


void MoCapCortex::convertCortexMarkerSetToNatNet(sBodyData& refCortex, sMarkerSetData& refNatNet)
{
	if (refCortex.nMarkers != refNatNet.nMarkers)
	{
		LOG_ERROR("Mismatch in marker count");
		return;
	}

	for (int mIdx = 0; mIdx < refCortex.nMarkers; mIdx++)
	{
		convertCortexMarkerToNatNet(refCortex.Markers[mIdx], refNatNet.Markers[mIdx]);
	}
}


void MoCapCortex::convertCortexSegmentToNatNet(double refCortex[], sRigidBodyData& refNatNet)
{
	Vector3D   pos;
	Quaternion rot;

	if (refCortex[0] < XEMPTY) // check for valid data
	{
		pos.x = (float)refCortex[0] * unitScaleFactor;
		pos.y = (float)refCortex[1] * unitScaleFactor;
		pos.z = (float)refCortex[2] * unitScaleFactor;

		// convert Euler rotation from Cortex (default: ZYX) to Quaternion
		Quaternion rotX(1, 0, 0, (float)RADIANS(refCortex[3])); // rotX
		Quaternion rotY(0, 1, 0, (float)RADIANS(refCortex[4])); // rotY
		Quaternion rotZ(0, 0, 1, (float)RADIANS(refCortex[5])); // rotZ
		rot.mult(rotZ).mult(rotY).mult(rotX);

		refNatNet.params    = 0x01; // tracking OK
		refNatNet.MeanError = (refCortex[0] < XEMPTY) ?
		                        (float)refCortex[6] * unitScaleFactor : // ATTENTION: Abusing mean error for bone length
		                        0.0f;
	}
	else
	{
		// segment data not available -> use origin and neutral pose
		refNatNet.params    = 0x00; // tracking not OK
		refNatNet.MeanError = 0.0f; // no "bone length"
	}

	refNatNet.x = pos.x;
	refNatNet.y = pos.y;
	refNatNet.z = pos.z;

	refNatNet.qx = rot.x;
	refNatNet.qy = rot.y;
	refNatNet.qz = rot.z;
	refNatNet.qw = rot.w;
}


void MoCapCortex::convertCortexSegmentsToNatNet(sBodyData& refCortex, sSkeletonData& refNatNet)
{
	if (refCortex.nSegments != refNatNet.nRigidBodies)
	{
		LOG_ERROR("Mismatch in segment count");
		return;
	}

	for (int sIdx = 0; sIdx < refCortex.nSegments; sIdx++)
	{
		convertCortexSegmentToNatNet(refCortex.Segments[sIdx], refNatNet.RigidBodyData[sIdx]);
	}
}


#endif // #ifdef USE_CORTEX
