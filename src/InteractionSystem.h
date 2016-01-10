#pragma once

#include "XBeeDevice.h"
#include "MoCapData.h"

#include <thread>


/**
 * Class for a single device data channel with a name and a value.
 */
class Channel
{
public:
	/**
	 * Creates a channel with a given name and a default value of 0.
	 *
	 * @param refName  the name of the channel
	 */
	Channel(const std::string& refName) : name(refName), value(0.0f) { };

public:
	const std::string name;
	      float       value;
};



/**
 * Abstract base class for a single interaction device and its data channels.
 */
class InteractionDevice
{
public:

	/**
	 * Creates an interaction device instance.
	 *
	 * @param name  the name of the device
	 */
	InteractionDevice(const std::string& name);

	virtual ~InteractionDevice() { }

	/**
	 * Gets the name of the interaction device.
	 *
	 * @return  the name of the device
	 */
	const std::string& getName() const;

	/**
	 * Gets the number of channels.
	 *
	 * @return  the number of channels
	 */
	size_t getChannelCount() const;

	/**
	 * Gets the list of data channels
	 *
	 * @return  the list of data channels
	 */
	const std::vector<Channel>& getChannels() const;

	/**
	 * Updates the data from a received packet.
	 *
	 * @param  refPacket  the received packet
	 *
	 * @return <code>true</code> if the packet was parsed successfully
	 */
	virtual bool update(const XBeePacket_Receive& refPacket) = 0;

protected:

	std::string          m_deviceName;
	std::vector<Channel> m_arrChannels;

};



/**
 * Class for a Joystick interaction device.
 */
class InteractionDevice_Joystick : public InteractionDevice
{
public:

	/**
	* Creates a Joystick interaction device instance.
	*
	* @param name       the name of the device
	* @param refDevice  the XBee device for the joystick
	*/
	InteractionDevice_Joystick(const std::string& name, XBeeRemoteDevice& refDevice);

	virtual ~InteractionDevice_Joystick() { }

	virtual bool update(const XBeePacket_Receive& refPacket);

protected:

	XBeeRemoteDevice& m_device;

};



/**
 * Class for managing interaction devices based on the XBee controllers.
 */
class InteractionSystem
{
public:

	/**
	 * Creates an interaction system using a specific serial port.
	 *
	 * @param pPort  the serial port to use
	 */
	InteractionSystem(std::unique_ptr<SerialPort>& pPort);

	/**
	 * Deinitialises and destroys the interaction system.
	 */
	~InteractionSystem();

	/**
	 * Initialises the system by opening the serial port and polling for connected devices.
	 *
	 * @return <code>true</code> if initialisation was succesful
	 */
	bool initialise();

	/**
	 * Checks if the interaction system has been successfully initialised.
	 *
	 * @return <code>true</code> if initialisation was succesful
	 */
	bool isActive();

	/**
	 * Fills in the interaction device descriptions into the MoCap description structure.
	 *
	 * @param refData  the MoCap data structure to fill in
	 */
	void getSceneDescription(MoCapData& refData);

	/**
	 * Fills in the interaction device data into the MoCap data structure.
	 *
	 * @param refData  the MoCap data structure to fill in
	 */
	void getFrameData(MoCapData& refData);
	
	/**
	 * Deinitialises the system by closing the serial port and releasing it.
	 *
	 * @return <code>true</code> if deinitialisation was succesful
	 */
	bool deinitialise();

protected:

	/**
	 * Thread that receives data from the XBee devices in the background.
	 */
	void receiverThread();

protected:

	std::unique_ptr<SerialPort>      m_pSerialPort;
	std::unique_ptr<XBeeCoordinator> m_pCoordinator;
	std::thread                      m_receiverThread;

	std::vector<std::unique_ptr<InteractionDevice>> m_arrDevices;

};

