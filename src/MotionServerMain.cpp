/**
 * Motion Server main functions and server thread.
 *
 * (C) 2014-2016 by Stefan Marks
 * Auckland University of Technology
 */


// Server version information
const int arrServerVersion[4] = { 1, 7, 7, 0 };


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
#include "MoCapData.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MotionServer" 

#ifdef USE_CORTEX
#include "MoCapCortex.h"
#endif

#ifdef USE_OCULUS_RIFT
#include "MoCapOculusRift.h"
#endif

#include "MoCapSimulator.h"
#include "MoCapFile.h"
#include "InteractionSystem.h"


///////////////////////////////////////////////////////////////////////////////
// Configuration variables
//

struct sConfiguration 
{
	std::string strServerName;

	bool        useMulticast;
	std::string strNatNetServerAddress;
	std::string strNatNetServerMulticastAddress;
	int         iNatNetCommandPort;
	int         iNatNetDataPort;

	bool        writeData;
	std::string dataFilename;

	bool        useCortex;
	std::string strRemoteCortexAddress;
	std::string strLocalCortexAddress;

	int         iInteractionControllerPort;

	bool        useOculusRift;

	sConfiguration()
	{
		// default configuration

		strServerName                   = "MotionServer";

		useMulticast                    = false;
		strNatNetServerAddress          = "127.0.0.1";
		strNatNetServerMulticastAddress = "";
		
		// Portnumbers: Default is 1510/1511, but that seems to collide with Cortex.
		// 1503 is taken by Windows messenger, 1512 is taken by WINS
		// -> so let's use 1508, 1509
		iNatNetCommandPort = 1508;
		iNatNetDataPort    = 1509;

		iInteractionControllerPort = 0;

		writeData    = false;
		dataFilename = "";

		useCortex              = true;
		strRemoteCortexAddress = "127.0.0.1";
		strLocalCortexAddress  = strRemoteCortexAddress;

		useOculusRift = false;
	}

} config;


///////////////////////////////////////////////////////////////////////////////
// Operational variables
//

// Server variables
NatNetServer* pServer          = NULL;
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

void mocapTimerThread();


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void printUsage()
{
	std::cout << "Command line arguments:" << std::endl
		<< "-h                                    Print Help" << std::endl
		<< "-serverName <name>                    Name of MoCap Server (default: 'MotionServer')" << std::endl
		<< "-serverAddr <address>                 IP Address of MotionServer (default: 127.0.0.1)" << std::endl
		<< "-multicastAddr <address>              IP Address of multicast MotionServer (default: Unicast)" << std::endl
#ifdef USE_OCULUS_RIFT
		<< "-noHMD                                No Oculus Rift detection" << std::endl
#endif
#ifdef USE_CORTEX
		<< "-cortexRemoteAddr <address>           IP Address of remote interface to connect to Cortex" << std::endl
		<< "-cortexLocalAddr <address>            IP Address of local interface to connect to Cortex" << std::endl
#endif
		<< "-interactionControllerPort <number>   COM port of XBee interaction controller (-1: scan)" << std::endl
		<< "-readFile <filename>                  Read and loop MoCap Data from a file" << std::endl
		<< "-writeFile                            Write MoCap Data into timestamped files" << std::endl
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
			else if (strArg == "-writefile")
			{
				config.writeData = true;
			}
#ifdef USE_OCULUS_RIFT
			else if (strArg == "-nohmd")
			{
				config.useOculusRift = false;
			}
#endif
		}
		// check arguments with one additional parameter
		if (argIdx + 1 < nArguments)
		{
			// convert parameter to UTF8
			std::wstring strParam1W(arrArguments[argIdx + 1]);
			std::string  strParam1(strParam1W.begin(), strParam1W.end());

			if (strArg == "-servername")
			{
				// Server name
				config.strServerName = strParam1;
			}
			else if (strArg == "-serveraddr")
			{
				// Server unicast address
				config.strNatNetServerAddress = strParam1;
			}
			else if (strArg == "-interactioncontrollerport")
			{
				// COM port number for XBee interaction controller
				config.iInteractionControllerPort = atoi(strParam1.c_str());
			}
			else if (strArg == "-multicastaddr")
			{
				// Server multicast address
				config.strNatNetServerMulticastAddress = strParam1;
				config.useMulticast = true;
			}
			else if (strArg == "-readfile")
			{
				// file to read
				config.dataFilename = strParam1;
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

	if (pSystem == NULL && !config.dataFilename.empty())
	{
		// query data file
		MoCapFileReader* pReader = new MoCapFileReader(config.dataFilename);
		if (pReader->initialise())
		{
			LOG_INFO("Reading MoCap data from file '" << config.dataFilename << "'");
			pSystem = pReader;
		}
		else
		{
			LOG_WARNING("Could not open file '" << config.dataFilename << "' for reading");
			pReader->deinitialise();
			delete pReader;
		}
	}

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
			LOG_WARNING("Cortex Server not found");
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
			LOG_WARNING("Oculus Rift not found");
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

	// are we supposed to write data into a file?
	if (config.writeData)
	{
		pMoCapFileWriter = new MoCapFileWriter(pSystem->getUpdateRate());
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

	// create new NatNet server
	mtxServer.lock();
	LOG_INFO("Creating server instance");

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
	pServer->SetVerbosityLevel(Verbosity_Info);
	pServer->SetErrorMessageCallback(callbackNatNetServerMessageHandler);

	if (iConnectionType == ConnectionType_Multicast)
	{
		pServer->SetMulticastAddress((char*) config.strNatNetServerMulticastAddress.c_str());
	}

	int retCode = pServer->Initialize(
		(char*) config.strNatNetServerAddress.c_str(),
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
		mtxServer.unlock();
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
			if (pInteractionSystem)
			{
				pInteractionSystem->getFrameData(*pMocapData);
			}

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
		
		pServer->SetMessageResponseCallback(NULL);
		pServer->Uninitialize();
		pServer->SetErrorMessageCallback(NULL);
		
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
			strcpy_s(pPacketOut->Data.Sender.szName, config.strServerName.c_str());
			for (int i = 0; i < 4; i++)
			{ 
				pPacketOut->Data.Sender.Version[i] = (unsigned char) arrServerVersion[i]; 
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
			else if (strRequestL == "getdatastreamaddress")
			{
				if ( config.useMulticast )
				{ 
					strcpy_s(pPacketOut->Data.szData, config.strNatNetServerMulticastAddress.c_str());
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
			LOG_INFO("Starting MotionServer '" << config.strServerName << "' v" 
				<< arrServerVersion[0] << "."
				<< arrServerVersion[1] << "."
				<< arrServerVersion[2] << "."
				<< arrServerVersion[3]);

			// create data object
			pMocapData = new MoCapData();

			// detect MoCap system?
			pMoCapSystem = detectMoCapSystem();

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
				pServer->SetMessageResponseCallback(NULL);

				// wait for streaming thread
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
			if (pMoCapFileWriter)
			{
				delete pMoCapFileWriter;
				pMoCapFileWriter = NULL;
			}

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
				LOG_INFO("Restarting MotionServer");
			}
		} 
		while (serverRestarting);
	}

	return 0;
}
