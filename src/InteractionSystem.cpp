#include "InteractionSystem.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "InteractionSystem"


///////////////////////////////////////////////////////////////////////////////

InteractionDevice::InteractionDevice(const std::string& name) :
	m_deviceName(name)
{
	// nothing else to do
}


const std::string& InteractionDevice::getName() const
{
	return m_deviceName;
}


const std::vector<std::string>& InteractionDevice::getChannelNames() const
{
	return m_arrChannelNames;
}


const std::vector<float>& InteractionDevice::getChannelValues() const
{
	return m_arrChannels;
}



///////////////////////////////////////////////////////////////////////////////

InteractionDevice_Joystick::InteractionDevice_Joystick(const std::string& name, XBeeRemoteDevice& refDevice) :
	InteractionDevice(name),
	m_device(refDevice)
{
	m_arrChannelNames.push_back("button1"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("button2"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("button3"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("button4"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("button5"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("button6"); m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("axis1");   m_arrChannels.push_back(0);
	m_arrChannelNames.push_back("axis2");   m_arrChannels.push_back(0);
}


bool InteractionDevice_Joystick::update(const XBeePacket_Receive& refPacket)
{
	bool success = false;
	// is this the right packet type
	if (refPacket.getFrameTypeID() == XBeePacket_IO_DataSample::FRAME_TYPE_ID) 
	{
		// is this from the correct device?
		const XBeePacket_IO_DataSample& sample = (const XBeePacket_IO_DataSample&) refPacket;
		if (sample.getNetworkAddress() == m_device.getNetworkAddress())
		{
			// yes > parse
			uint8_t pinState  = ((uint8_t)(sample.getDigitalInputState() & 0x00FF)); // only use lower 8 bits
			
			bool btnPrimary   = (pinState & 0x04) == 0; // pin 2: Primary Fire pressed
			bool btnSecondary = (pinState & 0x08) == 0; // pin 3: Secondary Fire pressed
			bool btnThumb     = (pinState & 0x40) == 0; // pin 6: Thumb buttons pressed
			bool btnStick     = (pinState & 0x80) == 0; // pin 7: Thumbstick moved
			uint8_t bitsThumb = (pinState & 0x30) >> 4; // pins 4 and 5: encoded thumb button/stick direction

			m_arrChannels[0] = btnPrimary   ? 1.0f : 0;
			m_arrChannels[1] = btnSecondary ? 1.0f : 0;

			m_arrChannels[2] = (btnThumb & (bitsThumb == 3)) ? 1.0f : 0; // BL
			m_arrChannels[3] = (btnThumb & (bitsThumb == 2)) ? 1.0f : 0; // BR
			m_arrChannels[4] = (btnThumb & (bitsThumb == 1)) ? 1.0f : 0; // TL
			m_arrChannels[5] = (btnThumb & (bitsThumb == 0)) ? 1.0f : 0; // TR

			if (btnStick)
			{
				// thumbstick pressed > translate into axis
				switch (bitsThumb)
				{
				case 0: // forward
					m_arrChannels[6] = 0;
					m_arrChannels[7] = +1;
					break;

				case 1: // left
					m_arrChannels[6] = -1;
					m_arrChannels[7] = 0;
					break;

				case 2: // right
					m_arrChannels[6] = +1;
					m_arrChannels[7] = 0;
					break;

				case 3: // backwards
					m_arrChannels[6] = 0;
					m_arrChannels[7] = -1;
					break;
				}
			}
			else
			{
				m_arrChannels[6] = 0;
				m_arrChannels[7] = 0;
			}

			/*
			LOG_INFO("update " << std::hex << (int) pinState << " " << btnPrimary << " " << btnSecondary
				<< " " << m_arrChannels[2] << " " << m_arrChannels[3]
				<< " " << m_arrChannels[4] << " " << m_arrChannels[5]
				<< " X:" << m_arrChannels[6] << ", Y:" << m_arrChannels[7]);
			*/

			success = true;
		}
	}
	return success;
}




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
				for each (auto& node in m_pCoordinator->getConnectedDevices())
				{
					if (!firstLine) output << std::endl;
					output << " - '" << node->getName() << "': "
						<< "Serial# " << std::hex << node->getSerialNumber()
						<< ", Address " << std::hex << node->getNetworkAddress()
						<< ", Type " << std::hex << node->getDeviceType()
						<< ", Parent " << std::hex << node->getParentAddress();
					firstLine = false;

					if (node->getName().find("oystick") != std::string::npos)
					{
						m_arrDevices.push_back(
							std::unique_ptr<InteractionDevice>(
								new InteractionDevice_Joystick(node->getName(), *node)));
					}
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


void InteractionSystem::getSceneDescription(MoCapData& refData)
{
	// TODO: implement
}


void InteractionSystem::getFrameData(MoCapData& refData)
{
	// TODO: implement
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
			for each (auto& device in m_arrDevices)
			{
				if (device->update(*packet))
				{
					// packet was parsed > no need to continue
					break;
				}
			}
		}
	}
	LOG_INFO("Receiver Thread stopped");
}


