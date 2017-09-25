/**
 * Motion Server main functions and server thread.
 *
 * (C) Sentience Lab (sentiencelab@aut.ac.nz), Auckland University of Technology, Auckland, New Zealand
 *
 */


/******************************************************************************
 * Includes
 */

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
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <mutex>

#include <tchar.h>


#pragma comment(lib, "NatNetLib.lib")
#include "NatNetTypes.h"
#include "NatNetServer.h"
#include "MoCapData.h"
#include "Configuration.h"
#include "Version.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MotionServer" 

#ifdef USE_CORTEX
#include "MoCapCortex.h"
#endif

#ifdef USE_KINECT
#include "MoCapKinect.h"
#endif

#ifdef USE_PIECEMETA
#include "MoCapPieceMeta.h"
#endif

#include "MoCapSimulator.h"
#include "MoCapFile.h"
#include "InteractionSystem.h"


/******************************************************************************
 * Configuration classes and variables
 */

class MotionServerConfiguration : public Configuration
{
public:

	MotionServerConfiguration() :
		Configuration("Motion Server"),
		printHelp(false),
		serverName("MotionServer"),
		useMulticast(false),
		serverAddress("127.0.0.1"),
		serverMulticastAddress(""),
		// Portnumbers: Default is 1510/1511, but that seems to collide with Cortex.
		// 1503 is taken by Windows messenger, 1512 is taken by WINS -> so let's use 1508, 1509
		commandPort(1508),
		dataPort(1509),
		interactionControllerPort(0),
		writeData(false),
		globalScale(1.0f)
	{
		addOption(   "-h",                                       "Print Help");
		addParameter("-serverName",                 "<name>",    "Name of MoCap Server (default: '" + serverName + "')");
		addParameter("-serverAddr",                 "<address>", "IP Address of MotionServer (default: " + serverAddress + ")");
		addParameter("-multicastAddr",              "<address>", "IP Address of multicast MotionServer (default: Unicast)");
		addParameter("-interactionControllerPort",  "<number>",  "COM port of XBee interaction controller (-1: scan)");
		addOption(   "-writeFile",                               "Write MoCap data into timestamped files");
		addParameter("-scale",                      "<scale>",   "Global scale for position data (default: 1.0)");
	}


	virtual bool handleArgument(unsigned int _idx, const std::string& _value)
	{
		bool success = true;
		std::istringstream strmValue(_value);
		switch (_idx)
		{
			case 0: // help
				printHelp = true;
				break;

			case 1: // Server name
				serverName = _value;
				break;

			case 2: // Server unicast address
				serverAddress = _value;
				break;
				
			case 3: // Server multicast address
				serverMulticastAddress = _value;
				useMulticast = true;
				break;

			case 4: // COM port number for XBee interaction controller
				strmValue >> interactionControllerPort;
				break;

			case 5: // write file
				writeData = true;
				break;

			case 6: // global scale factor
				strmValue >> globalScale;
				break;

			default:
				success = false;
				break;
		}
		return success;
	}

public:

	bool        printHelp;

	std::string serverName;

	bool        useMulticast;
	std::string serverAddress;
	std::string serverMulticastAddress;
	int         commandPort;
	int         dataPort;

	bool        writeData;

	int         interactionControllerPort;

	float       globalScale;
};


struct sConfiguration
{
	MotionServerConfiguration*    pMain;
	MoCapFileReaderConfiguration* pFileReader;

#ifdef USE_CORTEX
	MoCapCortexConfiguration*     pCortex;
#endif
	
#ifdef USE_KINECT
	MoCapKinectConfiguration*     pKinect;
#endif

#ifdef USE_PIECEMETA
	MoCapPieceMetaConfiguration*  pPieceMeta;
#endif

	std::vector<Configuration*> systemConfigurations;


	sConfiguration()
	{
		pMain = new MotionServerConfiguration();
		systemConfigurations.push_back(pMain);

		pFileReader = new MoCapFileReaderConfiguration();
		systemConfigurations.push_back(pFileReader);

#ifdef USE_CORTEX
		pCortex = new MoCapCortexConfiguration();
		systemConfigurations.push_back(pCortex);
#endif

#ifdef USE_KINECT
		pKinect = new MoCapKinectConfiguration();
		systemConfigurations.push_back(pKinect);
#endif

#ifdef USE_PIECEMETA
		pPieceMeta = new MoCapPieceMetaConfiguration();
		systemConfigurations.push_back(pPieceMeta);
#endif
	}

} config;


/******************************************************************************
 * Operational variables
 */

// Server variables
NatNetServer* pServer          = nullptr;
std::mutex    mtxServer;
bool          serverStarting   = true;
bool          serverRunning    = false;
bool          serverRestarting = false;
uint8_t       arrServerNatNetVersion[4]; // filled in later

// MoCap system variables
MoCapSystem*  pMoCapSystem;
std::mutex    mtxMoCap;
MoCapData*    pMocapData;
sPacket       packetOut;

MoCapFileWriter* pMoCapFileWriter;

// Interaction system variables
InteractionSystem* pInteractionSystem;

// Miscellaneous
// 
      int  frameCallbackCounter   = 0;  // counter for MoCap frame callbacks
      int  frameCallbackModulo    = 60; // counter modulo value to achieve 1Hz callback animation rate
      int  callbackAnimCounter    = 0;  // counter for current callback animation index
const char arrCallbackAnimation[] = { '-', '/', '|', '\\' }; // characters for the callback animation


///////////////////////////////////////////////////////////////////////////////
// Function prototypes
//

void parseCommandLine(const std::vector<std::string>& arguments);
bool createServer();
bool isServerRunning();
void signalNewFrame();
bool destroyServer();


/******************************************************************************
 * Callback/Thread prototypes
 */

void __cdecl callbackNatNetServerMessageHandler(int iMessageType, char* czMessage);
int  __cdecl callbackNatNetServerRequestHandler(sPacket* pPacketIn, sPacket* pPacketOut, void* pUserData);

void mocapTimerThread();


/******************************************************************************
 * Main functions
 */

void printUsage()
{
	const int w = 40;
	
	// go through all possible configurable systems
	for (std::vector<Configuration*>::const_iterator sys = config.systemConfigurations.cbegin();
		sys != config.systemConfigurations.cend();
		sys++)
	{
		std::cout << (*sys)->getSystemName() << " options:" << std::endl;
		// list their comand line options
		for (std::vector<Configuration::Argument>::const_iterator param = (*sys)->getArguments().cbegin();
			param != (*sys)->getArguments().cend();
			param++
			)
		{
			std::string p = " " + (*param).getName() + " " + (*param).getParameter();
			std::cout << std::left << std::setw(w) << p << (*param).getDescription() << std::endl;
		}
	}
}


/**
 * Parses the command line
 */
void parseCommandLine(const std::vector<std::string>& arguments)
{
	if (arguments.size() == 1)
	{
		// no command line option passed: print usage
		config.pMain->printHelp = true;
	}

	size_t argIdx = 1;
	while (argIdx < arguments.size())
	{
		// convert argument into lowercase
		std::string strArg = arguments[argIdx];
		std::string strArgLowercase;
		std::transform(strArg.begin(), strArg.end(), std::back_inserter(strArgLowercase), ::tolower);

		// check arguments with no additional parameter
		if (argIdx < arguments.size())
		{
			// run option through systems
			for (auto iterConfig = config.systemConfigurations.cbegin();
				iterConfig != config.systemConfigurations.cend();
				iterConfig++)
			{
				(*iterConfig)->processOption(strArgLowercase);
			}
		}
		// check arguments with one additional parameter
		if (argIdx + 1 < arguments.size())
		{
			// get parameter
			std::string strParam1(arguments[argIdx + 1]);
			// run option through systems
			for (auto iterConfig = config.systemConfigurations.cbegin();
				iterConfig != config.systemConfigurations.cend();
				iterConfig++)
			{
				(*iterConfig)->processParameter(strArgLowercase, strParam1);
			}
		}

		argIdx++; // next argument
	}
}


/**
 * Detects which MoCap system is active, e.g., Cortex, Kinect, etc.
 * As a fallback, a simulation system is used.
 *
 * @return  a pointer to the detected MoCap system instance
 *          or <code>nullptr</code> if no system was detected
 */
MoCapSystem* detectMoCapSystem()
{
	MoCapSystem* pSystem = nullptr;

	if (pSystem == nullptr && !config.pFileReader->filename.empty())
	{
		// query data file
		MoCapFileReader* pReader = new MoCapFileReader(*config.pFileReader);
		if (pReader->initialise())
		{
			LOG_INFO("Reading MoCap data from file '" << config.pFileReader->filename << "'");
			pSystem = pReader;
		}
		else
		{
			LOG_WARNING("Could not open file '" << config.pFileReader->filename << "' for reading");
			pReader->deinitialise();
			delete pReader;
		}
	}

#ifdef USE_CORTEX
	if (pSystem == nullptr && config.pCortex->useCortex)
	{
		// query Cortex
		LOG_INFO("Querying Cortex Server");
		MoCapCortex* pCortex = new MoCapCortex(*config.pCortex);
		if ( pCortex->initialise() )
		{
			LOG_INFO("Cortex Server found");
			pSystem = pCortex;
		}
		else
		{
			LOG_WARNING("Cortex Server not found");
			pCortex->deinitialise();
			delete pCortex;
		}
	}
#endif

#ifdef USE_KINECT
	if (pSystem == nullptr && config.pKinect->useKinect)
	{
		// query Kinect sensors
		LOG_INFO("Querying Kinect sensors");

		MoCapKinect* pKinect = new MoCapKinect(*config.pKinect);
		if (pKinect->initialise())
		{
			LOG_INFO("Kinect sensor found");
			pSystem = pKinect;
		}
		else
		{
			LOG_WARNING("No Kinect sensors found");
			pKinect->deinitialise();
			delete pKinect;
		}
	}
#endif

#ifdef USE_PIECEMETA
	if (pSystem == nullptr && config.pPieceMeta->usePieceMeta)
	{
		MoCapPieceMeta* pPieceMeta = new MoCapPieceMeta(*config.pPieceMeta);
		if (config.pPieceMeta->listOnly)
		{
			// only list PieceMeta packages/channels, don't actually start the server
			pPieceMeta->initialise();
			serverStarting = false;
			pPieceMeta->deinitialise();
		}
		else
		{
			if (pPieceMeta->initialise())
			{
				LOG_INFO("PieceMeta database contacted");
				pSystem = pPieceMeta;
			}
			else
			{
				LOG_WARNING("Could not connect to PieceMeta database");
				pPieceMeta->deinitialise();
				delete pPieceMeta;
			}
		}
	}
#endif

	return pSystem;
}


/**
 * Detects the XBee interaction system controller.
 *
 * @return  the controller instance
 *          (or <code>nullptr</code> if no controller was found)
 */
InteractionSystem* detectInteractionSystem()
{
	InteractionSystem* pSystem = nullptr;
	
	if (config.pMain->interactionControllerPort > 255)
	{
		// invalid > don't use
		config.pMain->interactionControllerPort = 0;
	}

	int scanFrom = config.pMain->interactionControllerPort;
	int scanTo   = config.pMain->interactionControllerPort + 1;
	if (config.pMain->interactionControllerPort < 0)
	{
		scanFrom = 1;
		scanTo = 256;
		LOG_INFO("Scanning for Interaction System...");
	}
	else if (config.pMain->interactionControllerPort > 0)
	{
		LOG_INFO("Searching Interaction System on COM" << config.pMain->interactionControllerPort);
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
				pSystem = nullptr;
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
 * @return <code>true</code> when server was created, 
 *         <code>false</code> when not.
 */
bool createServer()
{
	// don't connect twice
	if (isServerRunning())
	{
		destroyServer(); 
	}

	// create new NatNet server
	mtxServer.lock();
	LOG_INFO("Creating server instance");

	const int iConnectionType = config.pMain->useMulticast ? ConnectionType_Multicast : ConnectionType_Unicast;
	pServer = new NatNetServer(iConnectionType);

	// print version info
	pServer->NatNetVersion(arrServerNatNetVersion);
	LOG_INFO("NatNet Server version v"
	         << (int) arrServerNatNetVersion[0] << "."
	         << (int) arrServerNatNetVersion[1] << "."
	         << (int) arrServerNatNetVersion[2] << "."
	         << (int) arrServerNatNetVersion[3]);

	// set callbacks
	pServer->SetVerbosityLevel(Verbosity_Info);
	pServer->SetErrorMessageCallback(callbackNatNetServerMessageHandler);

	if (iConnectionType == ConnectionType_Multicast)
	{
		pServer->SetMulticastAddress((char*) config.pMain->serverMulticastAddress.c_str());
	}

	int retCode = pServer->Initialize(
		(char*) config.pMain->serverAddress.c_str(),
		config.pMain->commandPort,
		config.pMain->dataPort);

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
		mtxServer.unlock();
		destroyServer();
	}

	mtxServer.unlock();

	return isServerRunning();
}


/**
 * Checks if the NatNet server is running.
 *
 * @return <code>true</code> if server is running, <code>false</code> if not
 */
bool isServerRunning()
{
	return (pServer != nullptr);
}


/**
 * Called from MoCap subsystems when they actively provide a new frame.
 *
 * @return <code>true</code> if server is streaming, <code>false</code> if not
 */
void signalNewFrame()
{
	mtxMoCap.lock();
	if (pMoCapSystem && pMoCapSystem->isActive() && pMocapData)
	{
		if (pMoCapSystem->getFrameData(*pMocapData))
		{
			if (pInteractionSystem)
			{
				pInteractionSystem->getFrameData(*pMocapData);
			}

			pMocapData->applyScale(config.pMain->globalScale);

			mtxServer.lock();
			if (pServer)
			{
				pServer->PacketizeFrameOfMocapData(&(pMocapData->frame), &packetOut);
				pServer->SendPacket(&packetOut);
			}
			mtxServer.unlock();

			if (pMoCapFileWriter)
			{
				pMoCapFileWriter->writeFrameData(*pMocapData);
			}
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
		frameCallbackCounter = (frameCallbackCounter + 1) % frameCallbackModulo;
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
		
		pServer->SetMessageResponseCallback(nullptr);
		pServer->Uninitialize();
		pServer->SetErrorMessageCallback(nullptr);
		
		delete pServer;
		pServer = nullptr;

		LOG_INFO("Server shut down");
		mtxServer.unlock();
	}
	return (pServer == nullptr);
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
			strcpy_s(pPacketOut->Data.Sender.szName, config.pMain->serverName.c_str());

			pPacketOut->Data.Sender.Version[0] = MOTIONSERVER_VERSION_MAJOR;
			pPacketOut->Data.Sender.Version[1] = MOTIONSERVER_VERSION_MINOR;
			pPacketOut->Data.Sender.Version[2] = MOTIONSERVER_VERSION_REVISION;
			pPacketOut->Data.Sender.Version[3] = 0;
	
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
			// Client does not typically poll for data, but we accommodate it here anyway
			// note: need to return response on same thread as caller

			// This function does not call pMoCapSystem->getFrameData()
			// because the streaming thread does that.
			// Additional polling might mess up the timing
			mtxServer.lock();
			if (pServer && pMocapData)
			{
				pServer->PacketizeFrameOfMocapData(&(pMocapData->frame), pPacketOut);
			}
			mtxServer.unlock();
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
				stopServer(); // TODO: Doesn't work yet due to std::cin waiting for Enter
			}
			else if (strRequestL == "restart")
			{
				restartServer(); // TODO: Doesn't work yet due to std::cin waiting for Enter
			}
			else if (strRequestL == "getframerate")
			{
				float rate = pMoCapSystem->getUpdateRate();
				sprintf_s(pPacketOut->Data.szData, "%.0f", rate);
				pPacketOut->nDataBytes = (unsigned short)strlen(pPacketOut->Data.szData) + 1;
			}
			else if (strRequestL == "getdatastreamaddress")
			{
				if ( config.pMain->useMulticast )
				{ 
					strcpy_s(pPacketOut->Data.szData, config.pMain->serverMulticastAddress.c_str());
				}
				else
				{
					strcpy_s(pPacketOut->Data.szData, "");
				}
				pPacketOut->nDataBytes = (unsigned short) strlen(pPacketOut->Data.szData) + 1;
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
void mocapTimerThread()
{
	// create variables to keep track of timing
	std::chrono::system_clock::time_point nextTick(std::chrono::system_clock::now() + std::chrono::milliseconds(100));

	while (serverRunning)
	{
		// sleep for a while
		std::this_thread::sleep_until(nextTick);
		// immediately calculate next tick to compensate for time the update() functions takes
		// read update rate from MoCap system in case it varies (e.g. file playback speed changed)
		std::chrono::milliseconds intervalTime((int)(1000.0 / pMoCapSystem->getUpdateRate()));
		nextTick += intervalTime;

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
#ifdef WIN32
int _tmain(int nArguments, _TCHAR* arrArguments[])
#else
int main(int nArguments, char* arrArguments[])
#endif
{
	#ifdef MONITOR_MEMORY_USAGE
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	// parse command line parameters
	// convert commandline arguments to array of std::string
	std::vector<std::string> commandlineArguments;
	for (int argIdx = 0; argIdx < nArguments; argIdx++)
	{
#ifdef WIN32
		//setup converter from WChar to UTF-8
		std::wstring argument(arrArguments[argIdx]);
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		commandlineArguments.push_back(converter.to_bytes(argument));
#else
		std::string argument(arrArguments[argIdx]);
		commandlineArguments.push_back(argument);
#endif
	}
	parseCommandLine(commandlineArguments);

	if (config.pMain->printHelp)
	{
		serverStarting   = false;
		serverRestarting = false;
		printUsage();
	}

	if (serverStarting)
	{
		do
		{
			LOG_INFO("Starting MotionServer '" << config.pMain->serverName << "' v"
				<< MOTIONSERVER_VERSION_MAJOR << "." 
				<< MOTIONSERVER_VERSION_MINOR << "." 
				<< MOTIONSERVER_VERSION_REVISION);

			// create data object
			pMocapData = new MoCapData();

			// detect MoCap system?
			pMoCapSystem = detectMoCapSystem();

			// check start flag again, because the init phase of MoCap systems might have changed it
			// e.g., PieceMeta -listOnly
			if (!serverStarting) break;

			if (pMoCapSystem == nullptr)
			{
				// fallback: use simulator
				LOG_INFO("No active motion capture systems found > Simulating");

				pMoCapSystem = new MoCapSimulator();
				pMoCapSystem->initialise();
			}

			// are we supposed to write data into a file?
			if (config.pMain->writeData)
			{
				pMoCapFileWriter = new MoCapFileWriter(pMoCapSystem->getUpdateRate());
			}

			// detect interaction system
			pInteractionSystem = detectInteractionSystem();

			// start server 
			if (createServer())
			{
				serverRunning    = true;
				serverRestarting = false;

				// prepare scene description
				if (pMoCapSystem)
				{
					pMoCapSystem->getSceneDescription(*pMocapData);
				}

				if (pInteractionSystem)
				{
					if (pMocapData->frame.nForcePlates == 0)
					{
						pInteractionSystem->getSceneDescription(*pMocapData);
					}
					else
					{
						// force platyes already defined (e.g., through file playback) > sorry, no realtime data possible
						LOG_WARNING("Cannot use real-time Interaction System data");
					}
				}

				// if enabled, write description to file
				if (pMoCapFileWriter)
				{
					pMoCapFileWriter->writeSceneDescription(*pMocapData);
				}

				// start responding to packets
				pServer->SetMessageResponseCallback(callbackNatNetServerRequestHandler);

				// start streaming thread
				float updateRate    = pMoCapSystem->getUpdateRate();
				frameCallbackModulo = (int) updateRate;
				std::thread streamingThread(mocapTimerThread);
				LOG_INFO("Streaming thread started (Update rate: " << updateRate << "Hz)");

				// is the global scale unusual?
				if ((config.pMain->globalScale < 0.99f) || (config.pMain->globalScale > 1.01f))
				{
					LOG_INFO("Global scale factor: " << config.pMain->globalScale);
				}

				// That's all folks
				LOG_INFO("MotionServer started");

				// construct possible command help output
				std::stringstream commands;
				commands
					<< std::endl << "\tq:Quit"
					<< std::endl << "\tr:Restart"
					<< std::endl << "\tp:Pause/Unpause"
					<< std::endl << "\td:Print Model Definitions"
					<< std::endl << "\tf:Print Frame Data";
				LOG_INFO("Commands:" << commands.str())

				do
				{
					LOG_INFO("Enter command:");
					// read a line of stdin
					char cmdBuf[256];
					std::cin.getline(cmdBuf, sizeof(cmdBuf));
					std::string strCommand(cmdBuf);

					// convert to lowercase
					std::string strCmdLowerCase;
					std::transform(strCommand.begin(), strCommand.end(), std::back_inserter(strCmdLowerCase), ::tolower);

					if ((strCmdLowerCase == "q") ||
					    (strCmdLowerCase == "quit"))
					{
						stopServer();
					}
					else if ((strCmdLowerCase == "r") ||
					         (strCmdLowerCase == "restart"))
					{
						restartServer();
					}
					else if (strCmdLowerCase == "p")
					{
						// pause/unpause
						bool running = pMoCapSystem->isRunning();
						pMoCapSystem->setRunning(!running);
						running = pMoCapSystem->isRunning();
						LOG_INFO((running ? "Resumed playback" : "Paused"));
					}
					else if (strCmdLowerCase == "d")
					{
						// print definitions
						std::stringstream strm;
						printModelDefinitions(strm, pMocapData->description);
						std::cout << strm.str() << std::endl;
					}
					else if (strCmdLowerCase == "f")
					{
						// print frame
						std::stringstream strm;
						printFrameOfData(strm, pMocapData->frame);
						std::cout << strm.str() << std::endl;
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

				LOG_INFO("Stopping MotionServer");

				// stop responding to packets
				pServer->SetMessageResponseCallback(nullptr);

				// wait for streaming thread
				streamingThread.join();

				LOG_INFO("Streaming thread stopped");
			}

			destroyServer();

			if (pInteractionSystem)
			{
				pInteractionSystem->deinitialise();
				delete pInteractionSystem;
				pInteractionSystem = nullptr;
			}
				
			// clean up structures and objects
			mtxMoCap.lock();
			if (pMoCapFileWriter)
			{
				delete pMoCapFileWriter;
				pMoCapFileWriter = nullptr;
			}

			if (pMoCapSystem)
			{
				pMoCapSystem->deinitialise();
				delete pMoCapSystem;
				pMoCapSystem = nullptr;
			}

			if (pMocapData)
			{
				delete pMocapData;
				pMocapData = nullptr;
			}
			mtxMoCap.unlock();

			if (serverRestarting)
			{
				LOG_INFO("Restarting MotionServer");
			}
		} 
		while (serverRestarting);
	}

	return 0;
}
