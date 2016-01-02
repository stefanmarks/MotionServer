/**
 * Class for managing serial port connections and packet based communication.
 */
#pragma once

#include <Windows.h>
#include <string>


class SerialPort
{
public:

	/**
	 * Creates a serial port COMx with the port number x.
	 *
	 * @param portNumber  the number of the COM port
	 */
	SerialPort(int portNumber);

	/**
	 * Checks if the COM port exists at all.
	 * Note: This does not automatically mean that it can be opened.
	 *
	 * @return <code>true</code> if the port exists
	 */
	bool exists() const;

	/**
	 * Opens the serial port for reading/writing.
	 *
	 * @return <code>true</code> if the port could be opened
	 */
	bool open();

	/**
	 * Checks if the serial port is open.
	 *
	 * @return <code>true</code> if the port is open
	 */
	bool isOpen() const;

	/**
	 * Explicitely closes the serial port.
	 *
	 * @return <code>true</code> if the port was closed
	 */
	bool close();

	/**
	 * Sets the baudrate of the serial port
	 *
	 * @param baudrate  the baudrate to use
	 *
	 * @return <code>true</code> if the baudrate was changed successfully
	 */
	bool setBaudrate(DWORD baudRate);

	/**
	 * Gets the read timeout of the serial port.
	 *
	 * @return the read timeout in milliseconds
	 */
	DWORD getTimeout() const;

	/**
	 * Sets the read timeout of the serial port.
	 *
	 * @param timeout  the read timeout in milliseconds
	 *
	 * @return <code>true</code> if the timeout was changed successfully
	 */
	bool setTimeout(DWORD timeout);

	/**
	 * Sends data through the serial port.
	 *
	 * @param pBuffer       pointer to the data to send
	 * @param nBytesToSend  the amount of bytes to send
	 *
	 * @return the amount of bytes actually sent
	 */
	DWORD send(const void* pBuffer, DWORD nBytesToSend) const;

	/**
	 * Receives data from the serial port.
	 *
	 * @param pBuffer          pointer to the data to receive to (needs to be large enough)
	 * @param nBytesToReceive  the amount of bytes to receive
	 *
	 * @return the amount of bytes actually received. 
	 *         May be less than <code>nBytesToReceive</code> (or even 0) when a timeout happened
	 */
	DWORD receive(void* pBuffer, DWORD nBytesToReceive) const;

	/**
	 * Destructor for the serial port.
	 * This automatically closes the port if it was open.
	 */
	~SerialPort();

private:

	/**
	 * Function for handling errors in the OS functions
	 * and printing out a detailed error message.
	 *
	 * @param strFunctionName  the function that caused the error
	 */
	void handleError(const char* strFunctionName) const;

private:

	int          m_iPortNumber;  //< port number from the constructor
	std::string  m_strPortName;  //< short name, e.g., "COM1"
	std::string  m_strFileName;  //< Windows filename, e.g., "\\.\COM1"
	HANDLE       m_hPort;        //< Windows file handle to the serial port
};




