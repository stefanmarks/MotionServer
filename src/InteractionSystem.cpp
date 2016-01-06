#include "InteractionSystem.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "InteractionSystem"


InteractionSystem::InteractionSystem(std::unique_ptr<SerialPort>& pPort)
{
	m_pSerialPort = std::move(pPort);
}


InteractionSystem::~InteractionSystem()
{
	if (isActive())
	{
		deinitialise();
	}
}


bool InteractionSystem::initialise()
{
	if (m_pSerialPort && m_pSerialPort->open() && m_pSerialPort->isOpen())
	{
		m_pCoordinator.reset(new XBeeCoordinator(*m_pSerialPort));
		if (m_pCoordinator->isValid())
		{
			LOG_INFO("Initialised");
		}
	}
	return isActive();
}


bool InteractionSystem::isActive()
{
	return (m_pCoordinator && m_pCoordinator->isValid());
}


bool InteractionSystem::getSceneDescription(MoCapData& refData)
{
	return false;
}


bool InteractionSystem::getFrameData(MoCapData& refData)
{
	return false;
}


bool InteractionSystem::processCommand(const std::string& strCommand)
{
	return false;
}


bool InteractionSystem::deinitialise()
{
	if (isActive())
	{
		m_pCoordinator.reset(NULL);
		m_pSerialPort.reset(NULL);

		LOG_INFO("Deinitialised");
	}
	return true;
}