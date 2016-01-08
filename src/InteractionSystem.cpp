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
			// detect connected devices
			LOG_INFO("Scanning for devices...");
			m_pCoordinator->setNumberOfRetries(20);
			m_pCoordinator->scanDevices();
			if (!m_pCoordinator->getConnectedDevices().empty())
			{
				std::stringstream output;
				bool firstLine = true; // avoid unnecessary line after list
				for each (auto node in m_pCoordinator->getConnectedDevices())
				{
					if (!firstLine) output << std::endl;
					output << " - '" << node->getName() << "': "
						<< "Serial# " << std::hex << node->getSerialNumber()
						<< ", Address " << std::hex << node->getNetworkAddress()
						<< ", Type " << std::hex << node->getDeviceType()
						<< ", Parent " << std::hex << node->getParentAddress();
					firstLine = false;
				}
				LOG_INFO("Connected devices: " << std::endl << output.str());

				// start receiver thread
				m_receiverThread = std::thread(&InteractionSystem::receiverThread, this);
				LOG_INFO("Initialised");
			}
			else
			{
				LOG_INFO("No devices connected");
			}

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

		if (m_receiverThread.joinable())
		{
			m_receiverThread.join();
		}

		LOG_INFO("Deinitialised");
	}
	return true;
}


void InteractionSystem::receiverThread()
{
	LOG_INFO("Receiver Thread started");
	while (isActive())
	{
		// start receiving
		std::unique_ptr<XBeePacket_Receive> packet = m_pCoordinator->receive();
		if (packet)
		{
			// IO sample?
			if (packet->getFrameTypeID() == XBeePacket_IO_DataSample::FRAME_TYPE_ID)
			{
				XBeePacket_IO_DataSample* sample =
					(XBeePacket_IO_DataSample*)(packet.get());
				LOG_INFO("IO sample from " << std::hex << sample->getSerialNumber()
					<< ", Addr " << std::hex << sample->getNetworkAddress()
					<< ", Mask " << std::hex << sample->getDigitalInputMask()
					<< ", State " << std::hex << sample->getDigitalInputState()
				);
			}
			else
			{
				LOG_WARNING("Received packet with frame type " << std::hex << packet->getFrameTypeID());
			}
		}
	}
	LOG_INFO("Receiver Thread stopped");
}


