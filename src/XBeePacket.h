/**
* Class for managing XBee and ZigBee packets.
* Packets can be prepared for sending, and parsed when received.
* The subclasses provide access to the specific parameters of each packet type.
*
* (C) 2014-2016 Stefan Marks
* Auckland University of Technology
*/

#pragma once

#include "XBeeData.h"

#include <memory>
#include <string>


/******************************************************************************
 * Base class for any XBee packet.
 * Encapsulates the frame type ID.
 */

class XBeePacket
{
public:

	const static uint8_t START_DELIMITER = 0x7E; // packet start delimiter for XBee packets

protected:

	/**
	 * Creates a base XBee packet instance.
	 *
	 * @param frameTypeID  the packet frame type ID
	 */
	XBeePacket(uint8_t frameTypeID);

	/**
	 * Default virtual destructor (empty).
	 */
	virtual ~XBeePacket() { };

public:

	/**
	 * Gets the frame type ID of the packet.
	 *
	 * @return  frame type ID
	 */
	uint8_t getFrameTypeID() const;

protected:

	uint8_t m_frameTypeID;

};



/******************************************************************************
 * Base class for any XBee packet that can be sent.
 * Encapsulates the frame ID.
 */

class XBeePacket_Send : public XBeePacket
{
protected:

	/**
	 * Creates a base XBee packet instance.
	 *
	 * @param frameTypeID  the packet frame type ID
	 */
	XBeePacket_Send(uint8_t frameTypeID);

public:

	/**
	 * Default virtual destructor (empty).
	 */
	virtual ~XBeePacket_Send() { };

public:

	/**
	 * Gets the frame ID for the packet.
	 *
	 * @return  the frame ID to use (if applicable, otherwise 0)
	 */
	uint8_t getFrameID() const;

	/**
	 * Sets the frame ID for the packet.
	 *
	 * @param frameID  the frame ID to use (if applicable)
	 */
	virtual void setFrameID(uint8_t frameID);

	/**
	 * Adds the pure payload of the packet to a data buffer (frame type, data, ...).
	 * Start delimiter and length should have already been added, 
	 * checksum needs to be calculated and added afterwards.
	 *
	 * @param refBuffer  the buffer to fill
	 */
	virtual void marshal(XBeeWriteBuffer& refBuffer);

protected:

	uint8_t  m_frameID;

};



/******************************************************************************
 * Base class for any XBee packet that can be received.
 */

class XBeePacket_Receive : public XBeePacket
{
public:

	/**
	 * Creates a received XBee packet from a data buffer.
	 * The buffer needs to represent the full amount of the received data
	 * including start delimiter, length, frame type ID, etc.
	 *
	 * @param frameTypeID  the packet frame type ID
	 */
	XBeePacket_Receive(uint8_t frameTypeID);

	/**
	 * Default virtual destructor (empty).
	 */
	virtual ~XBeePacket_Receive() { };

	/**
	 * Extracts the data of the packet from a buffer.
	 *
	 * @param refBuffer  the buffer to read from
	 *
	 * @return <code>true</code> if unmarshalling was succesful,
	 *         <code>false</code> if an error occured
	 */
	virtual bool unmarshal(const XBeeReadBuffer& refBuffer);

	/**
	 * Gets the frame ID (if applicable).
	 *
	 * @return  the frame ID
	 */
	uint8_t getFrameID() const;

protected:

	uint8_t  m_frameID;

};



/******************************************************************************
 * Class for an XBee AT Command packet.
 */

class XBeePacket_AT_Command : public XBeePacket_Send
{
public:

	static const uint8_t FRAME_TYPE_ID = 0x08;

public:

	/**
	 * Creates an empty AT command packet.
	 */
	XBeePacket_AT_Command();

	/**
	 * Creates an AT command packet (no parameters).
	 *
	 * @param strCommand  AT Command to set
	 */
	XBeePacket_AT_Command(const std::string& strCommand);

	/**
	 * Default destructor (empty).
	 */
	virtual ~XBeePacket_AT_Command() { };

	/**
	 * Changes the AT command.
	 *
	 * @param strCommand the new AT command
	 */
	void setCommand(const std::string& strCommand);


	virtual void marshal(XBeeWriteBuffer& refBuffer);

protected:

	std::string m_strCommand;

};



/******************************************************************************
 * Class for an XBee AT Command Response packet.
 */

class XBeePacket_AT_CommandResponse : public XBeePacket_Receive
{
public:

	static const uint8_t FRAME_TYPE_ID = 0x88;

	// AT command status codes
	enum Status
	{
		OK               = 0x00,
		Error            = 0x01,
		InvalidCommand   = 0x02,
		InvalidParameter = 0x03,
		TX_Failure       = 0x04
	};

public:

	/**
	 * Creates an AT Command Response packet.
	 */
	XBeePacket_AT_CommandResponse();

	/**
	 * Default destructor (empty).
	 */
	virtual ~XBeePacket_AT_CommandResponse() { };

	virtual bool unmarshal(const XBeeReadBuffer& refBuffer);

	/**
	 * Gets the AT command that this is a response to.
	 *
	 * @return  the AT command name
	 */
	const std::string& getCommand() const;

	/**
	 * Gets the status of the response.
	 *
	 * @return status of the response
	 */
	Status getStatus() const;

	/**
	 * Checks is the response was OK.
	 *
	 * @return <code>true</code> if the reponse was OK
	 */
	bool isOK() const;

	/**
	 * Gets a 16 bit response result.
	 *
	 * @return a 16 bit result
	 */
	uint16_t getInt16() const;

	/**
	 * Gets a 32 bit response result.
	 *
	 * @return a 32 bit result
	 */
	uint32_t getInt32() const;

	/**
	 * Gets a string response result.
	 *
	 * @return a string result
	 */
	std::string getString() const;

	/**
	 * Gets the raw response result buffer.
	 *
	 * @return the response result buffer
	 */
	const XBeeReadBuffer& getRawData() const;

protected:

	std::string    m_strCommand;
	Status         m_status;
	XBeeReadBuffer m_bufData;

};



/******************************************************************************
* Class for an XBee Remote AT Command packet.
*/

class XBeePacket_RemoteAT_Command : public XBeePacket_AT_Command
{
public:

	static const uint8_t FRAME_TYPE_ID = 0x17;

public:

	/**
	 * Creates an AT command packet (no parameters).
	 *
	 * @param strCommand  AT Command to set
	 */
	XBeePacket_RemoteAT_Command(const std::string& strCommand);

	/**
	 * Default destructor (empty).
	 */
	virtual ~XBeePacket_RemoteAT_Command() { };

	/**
	 * Sets the serial number of the recipient of this AT command.
	 *
	 * @param serialNumber  the recipient's serial number
	 */
	void setSerialNumber(uint64_t serialNumber);

	/**
	 * Sets the network address of the recipient of this AT command.
	 *
	 * @param serialNumber  the recipient's serial number
	 */
	void setNetworkAddress(uint16_t networkAddress);


	virtual void marshal(XBeeWriteBuffer& refBuffer);

protected:

	uint64_t  m_serialNumber;
	uint16_t  m_networkAddress;

};



/******************************************************************************
 * Class for an XBee Remote AT Command Response packet.
 */

class XBeePacket_RemoteAT_CommandResponse : public XBeePacket_AT_CommandResponse
{
public:

	static const uint8_t FRAME_TYPE_ID = 0x97;

public:

	/**
	 * Creates a Remote AT Command Response packet.
	 */
	XBeePacket_RemoteAT_CommandResponse();

	/**
	 * Default destructor (empty).
	 */
	virtual ~XBeePacket_RemoteAT_CommandResponse() { };


	virtual bool unmarshal(const XBeeReadBuffer& refBuffer);

	/**
	 * Gets the serial number of the sender.
	 *
	 * @return  the serial number
	 */
	uint64_t getSerialNumber() const;

	/**
	 * Gets the network address of the sender.
	 *
	 * @return the network address of the sender
	 */
	uint16_t getNetworkAddress() const;

private:

	uint64_t  m_serialNumber;
	uint16_t  m_networkAddress;

};



/******************************************************************************
 * Class for an XBee IO data sample packet.
 */

class XBeePacket_IO_DataSample : public XBeePacket_Receive
{
public:

	static const uint8_t FRAME_TYPE_ID = 0x92;

public:

	/**
	 * Creates an IO Data Sample packet instance.
	 */
	XBeePacket_IO_DataSample();

	/**
	 * Default destructor (empty).
	 */
	virtual ~XBeePacket_IO_DataSample() { };


	virtual bool unmarshal(const XBeeReadBuffer& refBuffer);

	/**
	 * Gets the serial number of the sending module.
	 *
	 * @return  the serial number
	 */
	uint64_t getSerialNumber() const;

	/**
	 * Gets the network address of the sending module.
	 *
	 * @return  the 16 bit network address
	 */
	uint16_t getNetworkAddress() const;

	/**
	 * Gets the bitmask for the sampled input pins.
	 *
	 * @return bitmask for sampled input pins
	 */
	uint16_t getDigitalInputMask() const;

	/**
	 * Gets the state of the sampled input pins.
	 *
	 * @return state of the sampled input pins
	 */
	uint16_t getDigitalInputState() const;


private:

	uint64_t  m_serialNumber;
	uint16_t  m_networkAddress;

	uint16_t  m_digitalInputMask;
	uint16_t  m_digitalInputState;

};

