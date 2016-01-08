#include "XBeeDevice.h"
#include "XBeePacket.h"

#include <iostream>
#include <vector>

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "XBeeDevice"

#undef  LOG_DATA // define to get memory dumps of the received packets


///////////////////////////////////////////////////////////////////////////////
// XBeeDevice
//

XBeeDevice::XBeeDevice() :
	m_strName(""),
	m_serialNumber(0),
	m_networkAddress(0),
	m_versionSW(0),
	m_versionHW(0)
{
}


bool XBeeDevice::isValid() const
{
	// I was able to read the serial# > device seems to be OK
	return (m_serialNumber != 0);
}


const std::string& XBeeDevice::getName() const
{
	return m_strName;
}


uint64_t XBeeDevice::getSerialNumber() const
{
	return m_serialNumber;
}


uint16_t XBeeDevice::getSoftwareVersion() const
{
	return m_versionSW;
}


uint16_t XBeeDevice::getHardwareVersion() const
{
	return m_versionHW;
}


uint16_t XBeeDevice::getNetworkAddress() const
{
	return m_networkAddress;
}



///////////////////////////////////////////////////////////////////////////////
// XBeeCoordinator
//

XBeeCoordinator::XBeeCoordinator(SerialPort& refPort) :
	XBeeDevice(),
	m_serialPort(refPort),
	m_frameCounter(1),
	m_numOfRetries(3)
{
	// prepare serial port
	if (!m_serialPort.isOpen())
	{
		m_serialPort.open();
	}
	m_serialPort.setBaudrate(57600);
	m_serialPort.setTimeout(100);

	XBeePacket_AT_Command         command;
	XBeePacket_AT_CommandResponse response;

	// read serial number high...
	command.setCommand("SH");
	if (process(command, response))
	{
		m_serialNumber = ((uint64_t)response.getInt32()) << 32;
	}
	// ...and low
	command.setCommand("SL");
	if (process(command, response))
	{
		m_serialNumber |= (uint64_t)response.getInt32();
	}

	// read address
	command.setCommand("MY");
	if (process(command, response)) 
	{
		m_networkAddress = response.getInt16();
	}

	// read device identifier
	command.setCommand("NI");
	if (process(command, response))
	{
		m_strName = response.getString();
	}

	// read software version
	command.setCommand("VR");
	if (process(command, response)) 
	{
		m_versionSW = response.getInt16();
	}

	// read hardware version
	command.setCommand("HV");
	if (process(command, response)) 
	{
		m_versionHW = response.getInt16();
	}
}


XBeeCoordinator::~XBeeCoordinator()
{
	// clean up the list of nodes
	m_arrNodes.clear();

	// no need to hug the serial port any longer
	m_serialPort.close();
}


bool XBeeCoordinator::send(XBeePacket_Send& refPacket)
{
	// set FrameID
	refPacket.setFrameID(m_frameCounter);

	// advance frame ID counter for next package (never let it be 0)
	m_frameCounter = 1 + (m_frameCounter % 255);

	// create data buffer
	m_bufOut.clear();

	// Create buffer header
	// Delimiter first
	m_bufOut.addByte(XBeePacket::START_DELIMITER);
	// Packet length the next two bytes (0 as placeholder until finalisation)
	m_bufOut.addByte(0);
	m_bufOut.addByte(0);

	// now add the packet payload
	refPacket.marshal(m_bufOut);

	// fill in packet length (bytes 1 and 2) retrospectively
	uint16_t dataLen = (uint16_t)(m_bufOut.size() - 3);
	m_bufOut.setUInt16At(1, dataLen);

	// calculate and append checksum
	m_bufOut.addByte((uint8_t)255 - m_bufOut.calculateChecksum());

	// and now send the buffer
	DWORD nBytesToSend = m_bufOut.size();

#ifdef LOG_DATA
	LOG_INFO("Sending " << nBytesToSend << " bytes (frame ID " << (int)refPacket.getFrameID() << ")");
	printMemory(std::cout, m_bufOut.data(), m_bufOut.size());
#endif

	DWORD nSentBytes = m_serialPort.send(m_bufOut.data(), nBytesToSend);

	//	LOG_INFO("Sent " << nSentBytes << " bytes");

	return (nSentBytes == nBytesToSend);
}


std::unique_ptr<XBeePacket_Receive> XBeeCoordinator::receive()
{
	// create corresponding packet class instance
	std::unique_ptr<XBeePacket_Receive> pPacket = NULL;

	if (receivePacket())
	{
		// create specific packet subclass
		auto frameTypeID = m_bufIn.getByteAt(3);
		switch (frameTypeID)
		{
		case XBeePacket_AT_CommandResponse::FRAME_TYPE_ID:
			pPacket = std::make_unique<XBeePacket_AT_CommandResponse>();
			pPacket->unmarshal(m_bufIn);
			break;

		case XBeePacket_RemoteAT_CommandResponse::FRAME_TYPE_ID:
			pPacket = std::make_unique<XBeePacket_RemoteAT_CommandResponse>();
			pPacket->unmarshal(m_bufIn);
			break;

		case XBeePacket_IO_DataSample::FRAME_TYPE_ID:
			pPacket = std::make_unique<XBeePacket_IO_DataSample>();
			pPacket->unmarshal(m_bufIn);
			break;

		default:
			LOG_ERROR("Unhandled frame type 0x" << std::hex << (int)frameTypeID);
			break;
		}
	}

	return pPacket;
}


bool XBeeCoordinator::receive(XBeePacket_Receive& refReceive)
{
	bool success = false;
	bool retry   = true;
	int  retries = m_numOfRetries;

	while (!success && retry)
	{
		if (receivePacket())
		{
			auto frameTypeID = m_bufIn.getByteAt(3);
			if (frameTypeID == refReceive.getFrameTypeID())
			{
				success = refReceive.unmarshal(m_bufIn);
			}
			else
			{
				retries--;
				retry = (retries > 0);
				LOG_WARNING("Unexpected frame type 0x" << std::hex << (int)frameTypeID);
			}
		}
		else
		{
			// nothing received any more > get out
			retry = false;
		}
	}

	return success;
}


bool XBeeCoordinator::process(XBeePacket_Send& refSend, XBeePacket_Receive& refReceive)
{
	bool success = false;

	// send packet
	if (send(refSend))
	{
		// wait for response
		if (receive(refReceive))
		{
			// response received > is it the expected one?
			uint8_t idExpected = refSend.getFrameID();
			uint8_t idReceived = refReceive.getFrameID();
			if ((idExpected > 0) && (idExpected != idReceived))
			{
				LOG_ERROR("FrameID does not match (expected " << (int)idExpected
					<< ", received " << (int)idReceived << ")");
			}
			else
			{
				success = true;
			}
		}
	}

	return success;
}


void XBeeCoordinator::setNumberOfRetries(int retries)
{
	m_numOfRetries = max(1, retries);
}


int XBeeCoordinator::scanDevices()
{
	m_arrNodes.clear();
	DWORD oldTimeout = m_serialPort.getTimeout();

	XBeePacket_AT_Command         command("NT"); // read discovery timeout
	XBeePacket_AT_CommandResponse response;
	if (process(command, response))
	{
		// returned discovery timeout in in 100mx
		m_serialPort.setTimeout(response.getInt16() * 100);
	}

	// start discovery
	command.setCommand("ND");
	if (process(command, response))
	{
		// parse discovery results
		m_arrNodes.push_back(
			std::unique_ptr<XBeeRemoteDevice>(
				new XBeeRemoteDevice(*this, response.getRawData())));
	}

	m_serialPort.setTimeout(oldTimeout);

	return (int) m_arrNodes.size();
}


const std::vector<std::unique_ptr<XBeeRemoteDevice>>&  XBeeCoordinator::getConnectedDevices() const
{
	return m_arrNodes;
}


bool XBeeCoordinator::receivePacket()
{
	uint8_t start = 0;
	DWORD   rcvLen = 0;

	// is there anything at all?
	rcvLen = m_serialPort.receive(&start, 1);
	if (rcvLen < 1)
	{
		return false; // nope > get out
	}

	// is the first byte correct?
	if (start != XBeePacket::START_DELIMITER)
	{
		LOG_ERROR("Received response has invalid start delimiter (0x" << std::hex << (int)start << ")");
		return false;
	}

	// receive the length
	uint8_t len[2] = { 0, 0 };
	if (m_serialPort.receive(len, 2) != 2)
	{
		LOG_ERROR("Response without data length");
		return false;
	}

	// what is the size of the data?
	uint16_t dataLen = (((uint16_t)len[0]) << 8) | ((uint16_t)len[1]);
	// prepare receive buffer: data length +4: +1x delimiter +2x length bytes
	uint8_t* pBuffer = m_bufIn.prepareBuffer(dataLen);
	// receive (+3: receive the rest, +1: also receive checksum)
	rcvLen = m_serialPort.receive(pBuffer + 3, dataLen + 1);

#ifdef LOG_DATA
	printMemory(std::cout, m_bufIn.data(), rcvLen + 3);
#endif

	if (rcvLen != (dataLen + 1))
	{
		LOG_ERROR("Received packet incomplete (expected: " << dataLen << ", received " << rcvLen << ")");
		return false;
	}

	if (m_bufIn.calculateChecksum() != 255)
	{
		LOG_ERROR("Invalid checksum");
		return false;
	}

	return true;
}



///////////////////////////////////////////////////////////////////////////////
// XBeeRemoteDevice
//

XBeeRemoteDevice::XBeeRemoteDevice(XBeeCoordinator& refCoordinator, const XBeeReadBuffer& refBuffer) :
	XBeeDevice(),
	m_coordinator(refCoordinator)
{
	// Network discovery response (ND) comes in the following order:
	m_networkAddress = refBuffer.getUInt16At(0);  // MY (network address)
	m_serialNumber   = refBuffer.getNextUInt64(); // SH and SL (serial#)
	m_strName        = refBuffer.getNextString(); // NI (name)
	m_parentAddress  = refBuffer.getNextUInt16(); // Parent network addess
	m_deviceType     = (DeviceType)refBuffer.getNextByte(); // Device type
	//	STATUS<CR>(1 Byte: Reserved)
	//	PROFILE_ID<CR>(2 Bytes)
	//	MANUFACTURER_ID<CR>(2 Bytes)
}

uint16_t XBeeRemoteDevice::getParentAddress() const
{
	return m_parentAddress;
}


XBeeRemoteDevice::DeviceType XBeeRemoteDevice::getDeviceType() const
{
	return m_deviceType;
}


