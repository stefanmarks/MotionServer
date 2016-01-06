#pragma once

#include "XBeeDevice.h"
#include "MoCapData.h"


class InteractionSystem
{
public:
	InteractionSystem(std::unique_ptr<SerialPort>& pPort);

	~InteractionSystem();

	bool initialise();
	bool isActive();
	bool getSceneDescription(MoCapData& refData);
	bool getFrameData(MoCapData& refData);
	bool processCommand(const std::string& strCommand);
	
	bool deinitialise();

protected:

	std::unique_ptr<SerialPort>      m_pSerialPort;
	std::unique_ptr<XBeeCoordinator> m_pCoordinator;

};

