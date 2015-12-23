#include "MoCapCortex.h"
#include "Converter.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapCortex"


#ifdef USE_CORTEX

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
	//LOG_INFO("got new frame from callback");
	
	// process data
	// merely signal new frame, the main system will pick up the data through getFrameData(...)
	// which uses Cortex_GetCurrentFrame() 
	// which hopefully will have this frame data
	signalNewFrame();
}


MoCapCortex::MoCapCortex(const std::string &strCortexAddress, const std::string &strLocalAddress) :
	initialised(false),
	pCortexInfo(NULL)
{
	this->strCortexAddress = strCortexAddress;
	this->strLocalAddress  = strLocalAddress;
}


bool MoCapCortex::initialise()
{
	if (!initialised)
	{
		// print version info
		unsigned char cortexSDK_Version[4];
		Cortex_GetSdkVersion(cortexSDK_Version);
		LOG_INFO("Cortex SDK version v" << (int)cortexSDK_Version[1] << "."
		                                << (int)cortexSDK_Version[2] << "." << (int)cortexSDK_Version[3]);

		LOG_INFO("Connecting to Cortex server at address " << strCortexAddress
			<< (strLocalAddress.empty() ? "" : " from address ") << strLocalAddress);

		// set up callback handler for logging and streaming
		Cortex_SetErrorMsgHandlerFunc(callbackMoCapCortexMessageHandler);
		Cortex_SetDataHandlerFunc(callbackMoCapCortexDataHandler);

		// Cortex_SetClientCommunicationEnabled(true);

		if (Cortex_Initialize(
				(char*) (strLocalAddress.empty() ? NULL : strLocalAddress.c_str()), 
				(char*) strCortexAddress.c_str()
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

				// LOG_INFO("Cortex Communication to Clients is " << (Cortex_IsClientCommunicationEnabled() ? "enabled" : "disabled"));

				// determine unit conversion factor
				float unitToMillimeter = 1;
				void *pResponse = NULL;
				int  iResponseSize = 0;
				if (Cortex_Request("GetConversionToMillimeters", &pResponse, &iResponseSize) == RC_Okay)
				{
					unitToMillimeter = *((float*)pResponse);
					LOG_INFO("Units to millimeters: " << unitToMillimeter);
				}
				setUnitScaleFactor(unitToMillimeter / 1000.0f); // convert millimeters to units

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


int  MoCapCortex::getUpdateInterval()
{
	return 1000;
}


bool MoCapCortex::update()
{
	// nothing to do here
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


bool MoCapCortex::processCommand(const std::string& strCommand)
{
	bool processed = false;
	// nothing specific to do here (yet)
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

#endif // #ifdef USE_CORTEX