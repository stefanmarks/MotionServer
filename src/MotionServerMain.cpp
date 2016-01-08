/**
 * Motion Server main functions and server thread.
 *
 * (C) 2014-2016 by Stefan Marks
 * Auckland University of Technology
 */

///////////////////////////////////////////////////////////////////////////////
// includes

#include "Config.h"             // configuration definitions

#ifdef MONITOR_MEMORY_USAGE     // memory leak monitoring
	// source: https://msdn.microsoft.com/en-us/library/x98tx3cf%28v=vs.140%29.aspx

	#define _CRTDBG_MAP_ALLOC
	#include <stdlib.h>
	#include <crtdbg.h>
	
	#ifdef _DEBUG
		#ifndef DBG_NEW
			#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
			#define new DBG_NEW
		#endif
	#endif  // _DEBUG
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <mutex>

#include <tchar.h>


#pragma comment(lib, "NatNetLib.lib")
#include "NatNetTypes.h"
#include "NatNetServer.h"
#include "Converter.h"
#include "MoCapData.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MotionServer" 

#include "MoCapSimulator.h"

#ifdef USE_OCULUS_RIFT
#include "MoCapOculusRift.h"
#endif

#ifdef USE_CORTEX
#include "MoCapCortex.h"
#endif

#include "InteractionSystem.h"


///////////////////////////////////////////////////////////////////////////////
// Configuration variables
//

struct sConfiguration 
{
	bool        useMulticast;
	std::string strNatNetServerUnicastAddress;
	std::string strNatNetServerMulticastAddress;
	int         iNatNetCommandPort;
	int         iNatNetDataPort;

	bool        useCortex;
	std::string strRemoteCortexAddress;
	std::string strLocalCortexAddress;

	int         iInteractionControllerPort;

	bool        useOculusRift;

	sConfiguration()
	{
		// default configuration
		useMulticast                    = true;
		strNatNetServerUnicastAddress   = "127.0.0.1";
		strNatNetServerMulticastAddress = "224.0.0.1";
		
		// Portnumbers: Default is 1510/1511, but that seems to collide with Cortex.
		// 1503 is taken by Windows messenger, 1512 is taken by WINS
		// -> so let's use 1508, 1509
		iNatNetCommandPort = 1508;
		iNatNetDataPort    = 1509;

		iInteractionControllerPort = 0;

		useCortex              = true;
		strRemoteCortexAddress = "127.0.0.1";
		strLocalCortexAddress  = strRemoteCortexAddress;

		useOculusRift = true;
	}

} config;


///////////////////////////////////////////////////////////////////////////////
// Operational variables
//

// Server version information
std::string   strServerName       = "MotionServer";
const uint8_t arrServerVersion[4] = { 1, 6, 0, 2 };
      uint8_t arrServerNatNetVersion[4]; // filled in later

// Server variables
NatNetServer* pServer = NULL;
std::mutex    mtxServer;
bool serverStarting   = true;
bool serverRunning    = false;
bool serverRestarting = false;

// MoCap system variables
MoCapSystem*  pMoCapSystem;
std::mutex    mtxMoCap;
MoCapData*    pMocapData;
sPacket       packetOut;

// Interaction system variables
InteractionSystem* pInteractionSystem;

// Miscellaneous
// 
      int  frameCallbackCounter   = 0;  // counter for MoCap frame callbacks
      int  callbackAnimCounter    = 0;  // counter for current callback animation index
const char arrCallbackAnimation[] = { '-', '/', '|', '\\' }; // characters for the callback animation


///////////////////////////////////////////////////////////////////////////////
// Function prototypes
//

void parseCommandLine(int nArguments, _TCHAR* arrArguments[]);
bool createServer();
bool isServerRunning();
void signalNewFrame();
bool destroyServer();


///////////////////////////////////////////////////////////////////////////////
// Callback/Thread prototypes
//

void __cdecl callbackNatNetServerMessageHandler(int iMessageType, char* czMessage);
int  __cdecl callbackNatNetServerRequestHandler(sPacket* pPacketIn, sPacket* pPacketOut, void* pUserData);

void mocapTimerThread(int updateInterval);


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void printUsage()
{
	std::cout << "Command line arguments:" << std::endl
		<< "-h\t\t\tPrint Help" << std::endl
		<< "-serverAddr\t\tIP Address of MoCap Server IP address" << std::endl
		<< "-unicast\t\tUse Unicast data transfer (default)" << std::endl
		<< "-multicast\t\tUse Multicast data transfer" << std::endl
		<< "-multicastAddr\t\tIP Address of multicast MoCap Server IP address" << std::endl
#ifdef USE_OCULUS_RIFT
		<< "-noHMD\t\t\tNo Oculus Rift detection" << std::endl
#endif
#ifdef USE_CORTEX
		<< "-noCortex\t\tNo Cortex detection" << std::endl
		<< "-cortexRemoteAddr\tIP Address of remote interface to connect to Cortex" << std::endl
		<< "-cortexLocalAddr\tIP Address of local interface to connect to Cortex" << std::endl
#endif
		<< "-interactionControllerPort\tCOM port of XBee interaction controller (-1: scan)" << std::endl
	;
}


/**
 * Parses the command line
 */
void parseCommandLine(int nArguments, _TCHAR* arrArguments[])
{
	if (nArguments == 1)
	{
		// no command line option passed: print usage
		serverStarting = false;
		serverRestarting = false;
		printUsage();
	}

	int argIdx = 1;
	while (argIdx < nArguments)
	{
		// convert wchar argument into lowercase UTF8
		std::wstring strArgW(arrArguments[argIdx]);
		std::string  strArg;
		std::transform(strArgW.begin(), strArgW.end(), std::back_inserter(strArg), ::tolower);

		// check arguments with no additional parameter
		if (argIdx < nArguments)
		{
			if (strArg == "-h")
			{
				// help
				serverStarting   = false;
				serverRestarting = false;
				printUsage();
			}
			else if (strArg == "-unicast")
			{
				config.useMulticast = false;
			}
			else if (strArg == "-multicast")
			{
				config.useMulticast = true;
			}
#ifdef USE_OCULUS_RIFT
			else if (strArg == "-nohmd")
			{
				config.useOculusRift = false;
			}
#endif
#ifdef USE_CORTEX
			else if (strArg == "-nocortex")
			{
				config.useCortex = false;
			}
#endif
		}
		// check arguments with one additional parameter
		if (argIdx + 1 < nArguments)
		{
			// convert parameter to UTF8
			std::wstring strParam1W(arrArguments[argIdx + 1]);
			std::string  strParam1(strParam1W.begin(), strParam1W.end());

			if (strArg == "-serveraddr")
			{
				// Server unicast address
				config.strNatNetServerUnicastAddress = strParam1;
			}
			else if (strArg == "-multicastaddr")
			{
				// Server multicast address
				config.strNatNetServerMulticastAddress = strParam1;
				config.useMulticast = true;
			}
#ifdef USE_CORTEX
			else if (strArg == "-cortexremoteaddr")
			{
				// Cortex server address
				config.strRemoteCortexAddress = strParam1;
				config.useCortex = true;
			}
			else if (strArg == "-cortexlocaladdr")
			{
				// Cortex local address
				config.strLocalCortexAddress = strParam1;
				config.useCortex = true;
			}
#endif
			else if (strArg == "-interactioncontrollerport")
			{
				// COM port number for XBee interaction controller
				config.iInteractionControllerPort = atoi(strParam1.c_str());
			}
		}

		argIdx++; // next argument
	}
}


/**
 * Detects which MoCap system is active, e.g., Cortex, Oculus Rift, etc.
 * As a fallback, a simulation system is used.
 *
 * @return  the MoCap system instance
 */
MoCapSystem* detectMoCapSystem()
{
	MoCapSystem* pSystem = NULL;

#ifdef USE_CORTEX
	if (pSystem == NULL && config.useCortex)
	{
		// query Cortex
		LOG_INFO("Querying Cortex Server");
		MoCapCortex* pCortex = new MoCapCortex(config.strRemoteCortexAddress, config.strLocalCortexAddress);
		if ( pCortex->initialise() )
		{
			LOG_INFO("Cortex Server found");
			pSystem = pCortex;
		}
		else
		{
			LOG_INFO("Cortex Server not found");
			pCortex->deinitialise();
			delete pCortex;
		}
	}
#endif

#ifdef USE_OCULUS_RIFT
	if (pSystem == NULL && config.useOculusRift)
	{
		// query Oculus Rift
		LOG_INFO("Querying Oculus Rift");

		MoCapOculusRift* pHMD = new MoCapOculusRift();
		if (pHMD->initialise())
		{
			LOG_INFO("Oculus Rift found");
			pSystem = pHMD;
		}
		else
		{
			LOG_INFO("Oculus Rift not found");
			pHMD->deinitialise();
			delete pHMD;
		}
	}
#endif

	if (pSystem == NULL)
	{
		// fallback: use simulator
		LOG_INFO("No active motion capture systems found > Simulating");

		pSystem = new MoCapSimulator();
		pSystem->initialise();
	}

	return pSystem;
}


/**
* Detects the XBee interaction system controller.
*
* @return  the controller instance
*          (or <code>NULL</code> if no controller was found)
*/
InteractionSystem* detectInteractionSystem()
{
	InteractionSystem* pSystem = NULL;

	if (config.iInteractionControllerPort > 255)
	{
		// invalid > don't use
		config.iInteractionControllerPort = 0;
	}

	int scanFrom = config.iInteractionControllerPort;
	int scanTo   = config.iInteractionControllerPort + 1;
	if (config.iInteractionControllerPort < 0)
	{
		scanFrom = 1;
		scanTo = 256;
		LOG_INFO("Scanning for Interaction System...");
	}
	else if (config.iInteractionControllerPort > 0)
	{
		LOG_INFO("Searching Interaction System on COM" << config.iInteractionControllerPort);
	}

	// scan ports 
	for (int iPort = scanFrom; iPort < scanTo; iPort++)
	{
		if (iPort == 10) continue; // TODO: remove later (hack to avoid getting stuck on COM10 on my laptop)

		std::unique_ptr<SerialPort> pSerialPort(new SerialPort(iPort));
		if (pSerialPort->exists() && pSerialPort->open())
		{
			pSystem = new InteractionSystem(pSerialPort);
			if (pSystem->initialise())
			{
				LOG_INFO("Found Interaction System on COM" << iPort);
			} 
			else
			{
				pSystem->deinitialise();
				delete pSystem;
				pSystem = NULL;
			}
		}
	}

	if (!pSystem)
	{
		LOG_INFO("Cound not find Interaction System");
	}

	return pSystem;
}


/**
 * Creates the NatNet server instance.
 *
 * @return <code>true</code> when server was cerated, 
 *         <code>false</code> when not.
 */
bool createServer()
{
	// don't connect twice
	if (isServerRunning())
	{
		destroyServer(); 
	}

	LOG_INFO("Creating server instance");

	// create new NatNet server
	mtxServer.lock();

	const int iConnectionType = config.useMulticast ? ConnectionType_Multicast : ConnectionType_Unicast;
	pServer = new NatNetServer(iConnectionType);

	// print version info
	pServer->NatNetVersion(arrServerNatNetVersion);
	LOG_INFO("NatNet Server version v"
		     << (int) arrServerNatNetVersion[0] << "."
			 << (int) arrServerNatNetVersion[1] << "."
		     << (int) arrServerNatNetVersion[2] << "."
			 << (int) arrServerNatNetVersion[3]);

	// set callbacks
	pServer->SetMessageResponseCallback(callbackNatNetServerRequestHandler);
	pServer->SetVerbosityLevel(Verbosity_Info);
	pServer->SetErrorMessageCallback(callbackNatNetServerMessageHandler);

	if (iConnectionType == ConnectionType_Multicast)
	{
		pServer->SetMulticastAddress((char*) config.strNatNetServerMulticastAddress.c_str());
	}

	int retCode = pServer->Initialize(
		(char*) config.strNatNetServerUnicastAddress.c_str(),
		config.iNatNetCommandPort, 
		config.iNatNetDataPort);

	if (retCode == ErrorCode_OK)
	{
		LOG_INFO(((iConnectionType == ConnectionType_Multicast) ? "Multicast" : "Unicast") << " server initialised");
		// print address/port info
		char szDataIP_Address[256]      = ""; int iDataPort      = 0;
		char szCommandIP_Address[256]   = ""; int iCommandPort   = 0;
		char szMulticastIP_Address[256] = ""; int iMulticastPort = 0;
		pServer->GetSocketInfo(szDataIP_Address,      &iDataPort, 
		                       szCommandIP_Address,   &iCommandPort, 
		                       szMulticastIP_Address, &iMulticastPort);
		LOG_INFO("Command adress   : " << szCommandIP_Address << ":" << iCommandPort);
		LOG_INFO("Data adress      : " << szDataIP_Address    << ":" << iDataPort);
		if (iConnectionType == ConnectionType_Multicast)
		{
			LOG_INFO("Multicast address: " << szMulticastIP_Address << ":" << iMulticastPort);
		}
	}
	else
	{
		LOG_ERROR("Could not initialise server");
		destroyServer();
	}

	mtxServer.unlock();

	return isServerRunning();
}


/**
 * Checks if the NatNet server is running.
 *
 * @return TRUE if server is running, FALSE if not
 */
bool isServerRunning()
{
	return (pServer != NULL);
}


/**
 * Called from MoCap subsystems when they actively provide a new frame.
 *
 * @return TRUE if server is streaming, FALSE if not
 */
void signalNewFrame()
{
	mtxMoCap.lock();
	if (pMoCapSystem && pMoCapSystem->isActive() && pMocapData)
	{
		if (pMoCapSystem->getFrameData(*pMocapData))
		{
			mtxServer.lock();
			if (pServer)
			{
				pServer->PacketizeFrameOfMocapData(&(pMocapData->frame), &packetOut);
				pServer->SendPacket(&packetOut);
			}
			mtxServer.unlock();
		}
		else
		{
			LOG_ERROR("Could not retrieve signalled frame");
		}

		// display animated character
		if (frameCallbackCounter == 0)
		{
			callbackAnimCounter = (callbackAnimCounter + 1) % (sizeof(arrCallbackAnimation) / sizeof(arrCallbackAnimation[0]));
			std::cout << arrCallbackAnimation[callbackAnimCounter] << "\b" << std::flush;
		}
		frameCallbackCounter = (frameCallbackCounter + 1) % 60;
	}
	mtxMoCap.unlock();
}


/**
 * Stops the NatNet server thread.
 */
void stopServer()
{
	serverRunning    = false;
	serverRestarting = false;
}


/**
 * Stops and restarts the server thread.
 */
void restartServer()
{
	serverRunning    = false;
	serverRestarting = true;
}


/**
 * Shuts down the server and destroys the server instance.
 *
 * @return TRUE if server was shut down, FALSE if not
 */
bool destroyServer()
{
	if (isServerRunning())
	{
		mtxServer.lock();
		LOG_INFO("Shutting down server");
		pServer->Uninitialize();
		delete pServer;
		pServer = NULL;
		LOG_INFO("Server shut down");
		mtxServer.unlock();
	}
	return (pServer == NULL);
}


/**
 * Handler for messages from the NatNet server.
 */
void __cdecl callbackNatNetServerMessageHandler(int iMessageType, char* czMessage)
{
	switch (iMessageType)
	{
	case Verbosity_Error:
	case Verbosity_Warning:
		LOG_ERROR(czMessage);
		break;
	default:
		LOG_INFO(czMessage);
		break;
	}
}


/**
 * Handler for request packets from the NatNet server.
 */
int __cdecl callbackNatNetServerRequestHandler(sPacket* pPacketIn, sPacket* pPacketOut, void* pUserData)
{
	bool requestHandled = false;

	switch (pPacketIn->iMessage)
	{
		case NAT_PING:
		{
			LOG_INFO("Ping from client "
				<< pPacketIn->Data.Sender.szName << " v"
				<< (int)pPacketIn->Data.Sender.Version[0] << "."
				<< (int)pPacketIn->Data.Sender.Version[1] << "."
				<< (int)pPacketIn->Data.Sender.Version[2] << "."
				<< (int)pPacketIn->Data.Sender.Version[3] << ", NatNet v"
				<< (int)pPacketIn->Data.Sender.NatNetVersion[0] << "."
				<< (int)pPacketIn->Data.Sender.NatNetVersion[1] << "."
				<< (int)pPacketIn->Data.Sender.NatNetVersion[2] << "."
				<< (int)pPacketIn->Data.Sender.NatNetVersion[3]);

			// build server response packet
			pPacketOut->iMessage = NAT_PINGRESPONSE;
			pPacketOut->nDataBytes = sizeof(pPacketOut->Data.Sender);
			strcpy_s(pPacketOut->Data.Sender.szName, strServerName.c_str());
			for (int i = 0; i < 4; i++)
			{ 
				pPacketOut->Data.Sender.Version[i] = arrServerVersion[i]; 
			}
			for (int i = 0; i < 4; i++)
			{
				pPacketOut->Data.Sender.NatNetVersion[i] = arrServerNatNetVersion[i];
			}

			requestHandled = true;
			break;
		}

		case NAT_REQUEST_MODELDEF:
		{
			LOG_INFO("Requested scene description");
			mtxServer.lock();
			if (pServer)
			{
				pServer->PacketizeDataDescriptions(&(pMocapData->description), pPacketOut);
			}
			mtxServer.unlock();
			requestHandled = true;
			break;
		}

		case NAT_REQUEST_FRAMEOFDATA:
		{
			// Client does not typically poll for data, but we accomodate it here anyway
			// note: need to return response on same thread as caller
			mtxMoCap.lock();
			bool gotFrameOfData = (pMoCapSystem && pMocapData && pMoCapSystem->getFrameData(*pMocapData));
			mtxMoCap.unlock();
			if (gotFrameOfData)
			{
				mtxServer.lock();
				if (pServer)
				{
					pServer->PacketizeFrameOfMocapData(&(pMocapData->frame), pPacketOut);
				}
				mtxServer.unlock();
			}
			else
			{
				LOG_ERROR("Could not retrieve frame");
			}
			requestHandled = true;
			break;
		}

		case NAT_REQUEST:
		{
			std::string strRequest(pPacketIn->Data.szData);
			std::string strRequestL; // convert to all lowercase
			std::transform(strRequest.begin(), strRequest.end(), std::back_inserter(strRequestL), ::tolower);

			LOG_INFO("Client request '" << strRequest << "' received.");
			pPacketOut->iMessage = NAT_RESPONSE;
			pPacketOut->nDataBytes = 0;
			requestHandled = true; // assume best case first

			if (strRequestL == "quit")
			{
				stopServer();
			}
			else if (strRequestL == "restart")
			{
				restartServer();
			}
			else if (strRequestL == "getdatastreamaddress")
			{
				if ( config.useMulticast )
				{ 
					strcpy_s(pPacketOut->Data.szData, config.strNatNetServerMulticastAddress.c_str());
				}
				else
				{
					strcpy_s(pPacketOut->Data.szData, config.strNatNetServerUnicastAddress.c_str());
				}
				pPacketOut->nDataBytes = (unsigned short) strlen(pPacketOut->Data.szData) + 1;
			}
			else if (strRequestL == "enableunknownmarkers")
			{
				setHandleUnknownMarkers(true);
			}
			else if (strRequestL == "disableunknownmarkers")
			{
				setHandleUnknownMarkers(false);
			}
			else
			{
				// last resort: MoCap subsytem can handle this?
				mtxMoCap.lock();
				if (pMoCapSystem && pMoCapSystem->processCommand(strRequestL))
				{ 
					// success
				}
				else
				{
					pPacketOut->iMessage = NAT_UNRECOGNIZED_REQUEST;
					requestHandled = false;
				}
				mtxMoCap.unlock();
			}
			break;
		}

		default:
		{
			LOG_ERROR("Received invalid request " << pPacketIn->iMessage << " from client");
			pPacketOut->iMessage = NAT_UNRECOGNIZED_REQUEST;
			pPacketOut->nDataBytes = 0;
			break;
		}
	}

	return requestHandled;
}


/**
 * Timer thread for regularly sending frames
 *
 * @param updateInterval  the interval with which to update the data (in milliseconds)
 */
void mocapTimerThread(int updateInterval)
{
	std::chrono::milliseconds sleepTime = std::chrono::milliseconds(updateInterval);
	
	while (serverRunning)
	{
		std::this_thread::sleep_for(sleepTime);
		// mtxMoCap.lock(); < this would collide with the lock in signalNewFrame that is probably being called
		if (serverRunning && pMoCapSystem)
		{
			pMoCapSystem->update();
		}
		// mtxMoCap.unlock();
	}
}


/**
 * Main program
 */
int _tmain(int nArguments, _TCHAR* arrArguments[])
{
	#ifdef MONITOR_MEMORY_USAGE
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	// check for command line parameters
	parseCommandLine(nArguments, arrArguments);

	if (serverStarting)
	{
		do
		{
			LOG_INFO("Starting Motion Server v" 
				<< (int) arrServerVersion[0] << "."
				<< (int) arrServerVersion[1] << "."
				<< (int) arrServerVersion[2] << "."
				<< (int) arrServerVersion[3]);

			// create data object
			pMocapData = new MoCapData();

			// detect MoCap system?
			pMoCapSystem = detectMoCapSystem();

			// detect interaction system
			pInteractionSystem = detectInteractionSystem();

			// start server
			if ( createServer() )
			{
				serverRunning = true;
				serverRestarting = false;

				// prepare scene description
				if (pMoCapSystem)       pMoCapSystem->getSceneDescription(*pMocapData);
				if (pInteractionSystem) pInteractionSystem->getSceneDescription(*pMocapData);

				//unsigned int thread_id = 0;
				//_beginthreadex(NULL, 0, mocapTimerThread, NULL, 0, &thread_id);
				std::thread streamingThread(mocapTimerThread, pMoCapSystem->getUpdateInterval());
				LOG_INFO("Streaming thread started");

				LOG_INFO("Motion Server started");
				LOG_INFO("Commands:" << std::endl
					<< "\tq:Quit" << std::endl
					<< "\tr:Restart" << std::endl
					<< "\td:Print Model Definitions" << std::endl
					<< "\tf:Print Frame Data" << std::endl
					<< "\tu:Enable/Disable unknown markers")

				do
				{
					LOG_INFO("Enter command:");
					std::string strCommand;
					std::cin >> strCommand;

					std::string strCmdLowerCase;
					std::transform(strCommand.begin(), strCommand.end(), std::back_inserter(strCmdLowerCase), ::tolower);
					
					if (strCmdLowerCase == "q")
					{
						stopServer();
					}
					else if (strCmdLowerCase == "r")
					{
						restartServer();
					}
					else if (strCmdLowerCase == "d")
					{
						std::stringstream strm;
						printModelDefinitions(strm, pMocapData->description);
						std::cout << strm.str() << std::endl;
					}
					else if (strCmdLowerCase == "f")
					{
						std::stringstream strm;
						printFrameOfData(strm, pMocapData->frame);
						std::cout << strm.str() << std::endl;
					}
					else if (strCmdLowerCase == "u")
					{
						setHandleUnknownMarkers(!isHandlingUnknownMarkers());
						LOG_INFO("Unknown markers: " << (isHandlingUnknownMarkers() ? "enabled" : "disabled"));
					}
					else if (pMoCapSystem->processCommand(strCommand) == true)
					{
						// MoCap susbsytem was able to handle command
					}
					else
					{
						LOG_ERROR("Unknown command: '" << strCommand << "'");
					}
				} while (serverRunning);

				streamingThread.join();
				LOG_INFO("Streaming thread stopped");
			}

			destroyServer();

			if (pInteractionSystem)
			{
				pInteractionSystem->deinitialise();
				delete pInteractionSystem;
				pInteractionSystem = NULL;
			}

			// clean up structures and objects
			mtxMoCap.lock();
			if (pMoCapSystem)
			{
				pMoCapSystem->deinitialise();
				delete pMoCapSystem;
				pMoCapSystem = NULL;
			}

			if (pMocapData)
			{
				delete pMocapData;
				pMocapData = NULL;
			}
			mtxMoCap.unlock();

			if (serverRestarting)
			{
				LOG_INFO("Restarting Motion Server");
			}
		} 
		while (serverRestarting);
	}

	return 0;
}
