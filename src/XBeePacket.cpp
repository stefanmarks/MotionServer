#include "XBeePacket.h"
#include "XBeeDevice.h"


///////////////////////////////////////////////////////////////////////////////

XBeePacket::XBeePacket(uint8_t frameTypeID) :
	m_frameTypeID(frameTypeID)
{
	// nothing else to do here
}


uint8_t XBeePacket::getFrameTypeID() const
{
	return m_frameTypeID;
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_Send::XBeePacket_Send(uint8_t frameTypeID) :
	XBeePacket(frameTypeID),
	m_frameID(0)
{
	// nothing else to do here
}


uint8_t XBeePacket_Send::getFrameID() const
{
	return m_frameID;
}


void XBeePacket_Send::setFrameID(uint8_t frameID)
{
	m_frameID = frameID;
}


void XBeePacket_Send::marshal(XBeeWriteBuffer& refBuffer)
{
	// frame type ID always comes first
	refBuffer.addByte(getFrameTypeID());
	
	// there is usually more to it > implement in derived classes
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_Receive::XBeePacket_Receive(uint8_t frameTypeID) :
	XBeePacket(frameTypeID),
	m_frameID(0)
{
	// nothing else to do
}


bool XBeePacket_Receive::unmarshal(const XBeeReadBuffer& refBuffer)
{
	uint8_t frameTypeID = refBuffer.getByteAt(3);
	return (getFrameTypeID() == frameTypeID); // check if the frame type is as expected
}


uint8_t XBeePacket_Receive::getFrameID() const
{
	return m_frameID;
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_AT_Command::XBeePacket_AT_Command() :
	XBeePacket_Send(XBeePacket_AT_Command::FRAME_TYPE_ID),
	m_strCommand("")
{
	// nothing else to do
}


XBeePacket_AT_Command::XBeePacket_AT_Command(const std::string& strCommand) :
	XBeePacket_Send(XBeePacket_AT_Command::FRAME_TYPE_ID),
	m_strCommand(strCommand)
{
	// nothing else to do
}


void::XBeePacket_AT_Command::setCommand(const std::string& strCommand)
{
	m_strCommand = strCommand;
}


void XBeePacket_AT_Command::marshal(XBeeWriteBuffer& refBuffer)
{
	XBeePacket_Send::marshal(refBuffer);  // call parent method for filling in frame type
	refBuffer.addByte(m_frameID);         // frameID
	refBuffer.addString(m_strCommand, 2); // AT command letters
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_AT_CommandResponse::XBeePacket_AT_CommandResponse() :
	XBeePacket_Receive(XBeePacket_AT_CommandResponse::FRAME_TYPE_ID),
	m_status(XBeePacket_AT_CommandResponse::Status::OK),
	m_strCommand("")
{
	// nothing else to do
}


bool XBeePacket_AT_CommandResponse::unmarshal(const XBeeReadBuffer& refBuffer)
{
	bool success = false;
	if (XBeePacket_Receive::unmarshal(refBuffer))
	{
		m_frameID    = refBuffer.getByteAt(4);              // frame ID in pos 4
		m_strCommand = refBuffer.getNextString(2);          // AT command name in pos 5 and 6
		m_status     = (Status) refBuffer.getNextByte();    // command status in pos 7 
		m_bufData.copy(refBuffer, 8, refBuffer.size() - 1); // rest of data (-1 = checksum byte)
		success = true;
	}
	return success;
}


const std::string& XBeePacket_AT_CommandResponse::getCommand() const
{
	return m_strCommand;
}


bool XBeePacket_AT_CommandResponse::isOK() const
{
	return m_status == Status::OK;
}


XBeePacket_AT_CommandResponse::Status XBeePacket_AT_CommandResponse::getStatus() const
{
	return m_status;
}


uint16_t XBeePacket_AT_CommandResponse::getInt16() const
{
	// interpret the buffer as a 16 bit int (big endian)
	return m_bufData.getUInt16At(0);
}


uint32_t XBeePacket_AT_CommandResponse::getInt32() const
{
	// interpret the buffer as a 32 bit int (big endian)
	return m_bufData.getUInt32At(0);
}


std::string XBeePacket_AT_CommandResponse::getString() const
{
	// interpret the buffer as a string
	return m_bufData.getStringAt(0);
}


const XBeeReadBuffer& XBeePacket_AT_CommandResponse::getRawData() const
{
	return m_bufData;
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_RemoteAT_Command::XBeePacket_RemoteAT_Command(const std::string& strCommand) :
	XBeePacket_AT_Command(strCommand),
	m_serialNumber(0),
	m_networkAddress(0xFFFE) // "receiver unknown" address
{
	m_frameTypeID = XBeePacket_RemoteAT_Command::FRAME_TYPE_ID; // update this field
}


void::XBeePacket_RemoteAT_Command::setSerialNumber(uint64_t serialNumber)
{
	m_serialNumber = serialNumber;
}


void::XBeePacket_RemoteAT_Command::setNetworkAddress(uint16_t networkAddress)
{
	m_networkAddress = networkAddress;
}


void XBeePacket_RemoteAT_Command::marshal(XBeeWriteBuffer& refBuffer)
{
	XBeePacket_Send::marshal(refBuffer);   // call parent method for filling in frame type
	refBuffer.addByte(m_frameID);          // frameID
	refBuffer.addUInt64(m_serialNumber);   // serial number
	refBuffer.addUInt16(m_networkAddress); // network address
	refBuffer.addByte(0);                  // options (0 for now = nothing)
	refBuffer.addString(m_strCommand, 2);  // AT command letters
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_RemoteAT_CommandResponse::XBeePacket_RemoteAT_CommandResponse() :
	XBeePacket_AT_CommandResponse()
{
	m_frameTypeID = XBeePacket_RemoteAT_CommandResponse::FRAME_TYPE_ID;
}


bool XBeePacket_RemoteAT_CommandResponse::unmarshal(const XBeeReadBuffer& refBuffer)
{
	bool success = false;
	if (XBeePacket_Receive::unmarshal(refBuffer))
	{
		m_frameID        = refBuffer.getByteAt(4);     // frame ID in pos 4
		m_serialNumber   = refBuffer.getNextUInt64();  // serial number in pos 5
		m_networkAddress = refBuffer.getNextUInt16();  // network address in pos 13
		m_strCommand     = refBuffer.getNextString(2); // AT command name in pos 15 and 16
		m_status = (Status) refBuffer.getByteAt(17);   // command status in pos 17
		m_bufData.copy(refBuffer, 18, refBuffer.size() - 1); // rest of data (-1 = checksum byte)
		success = true;
	}
	return success;
}


uint64_t XBeePacket_RemoteAT_CommandResponse::getSerialNumber() const
{
	return m_serialNumber;
}


uint16_t XBeePacket_RemoteAT_CommandResponse::getNetworkAddress() const
{
	return m_networkAddress;
}



///////////////////////////////////////////////////////////////////////////////

XBeePacket_IO_DataSample::XBeePacket_IO_DataSample() :
	XBeePacket_Receive(XBeePacket_IO_DataSample::FRAME_TYPE_ID),
	m_serialNumber(0),
	m_networkAddress(0),
	m_digitalInputMask(0),
	m_digitalInputState(0)
{
	// nothing else to do
}


bool XBeePacket_IO_DataSample::unmarshal(const XBeeReadBuffer& refBuffer)
{
	bool success = false;
	if (XBeePacket_Receive::unmarshal(refBuffer))
	{
		m_serialNumber   = refBuffer.getUInt64At(4);    // pos 4: 64 bit serial#
		m_networkAddress = refBuffer.getNextUInt16();   // pos 12: 16 bit address

		m_digitalInputMask = refBuffer.getUInt16At(16); // pos 16: channel mask
		if (m_digitalInputMask > 0)
		{
			m_digitalInputState = refBuffer.getUInt16At(19); // pos 19: channel state
		}

		success = true;
	}
	return success;
}


uint64_t XBeePacket_IO_DataSample::getSerialNumber() const
{
	return m_serialNumber;
}


uint16_t XBeePacket_IO_DataSample::getNetworkAddress() const
{
	return m_networkAddress;
}


uint16_t XBeePacket_IO_DataSample::getDigitalInputMask() const
{
	return m_digitalInputMask;
}


uint16_t XBeePacket_IO_DataSample::getDigitalInputState() const
{
	return m_digitalInputState;
}

