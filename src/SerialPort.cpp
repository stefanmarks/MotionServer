
#include "SerialPort.h"
#include <ios>
#include <locale>

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "SerialPort"


SerialPort::SerialPort(int portNumber) :
	m_iPortNumber(portNumber),
	m_hPort(0)
{
	// build short name from port number
	std::stringstream strPortName;
	strPortName << "COM" << m_iPortNumber;
	m_strPortName = strPortName.str();

	// build filename from port number
	std::stringstream strFileName;
	strFileName << "\\\\.\\COM" << m_iPortNumber;
	m_strFileName = strFileName.str();
}


bool SerialPort::exists() const
{
	// source: http://stackoverflow.com/questions/1205383/listing-serial-com-ports-on-windows

	std::wstring strPortNameW(m_strPortName.begin(), m_strPortName.end()); // convert to wchar
	COMMCONFIG config;
	DWORD      configSize = sizeof(config);
	// Port exists if configuration can be retrieved (returns true)
	// or the config structure size changes to indicate it was too small.
	return (GetDefaultCommConfigW(strPortNameW.c_str(), &config, &configSize) == TRUE) ||
	       (configSize != sizeof(config));
}


bool SerialPort::open()
{
	if (!isOpen())
	{
		std::wstring strFileNameW(m_strFileName.begin(), m_strFileName.end()); // convert to wchar
		m_hPort = ::CreateFile(
			strFileNameW.c_str(),
			GENERIC_READ | GENERIC_WRITE, // access rights
			0,                            // don't share port
			0,                            // no security flags necessary
			OPEN_EXISTING,                // must exist to be opened
			0,					          // no overlapped operation
			0                             // no template file for COM ports
			);

		if (m_hPort == INVALID_HANDLE_VALUE)
		{
			handleError("opening serial port");
			m_hPort = 0;
		}
		else
		{
			LOG_INFO("Opened serial port " << m_strPortName);
		}
	}

	return isOpen();
}


bool SerialPort::isOpen() const
{
	return (m_hPort != 0);
}


bool SerialPort::close()
{
	if (isOpen())
	{
		if (::CloseHandle(m_hPort))
		{
			LOG_INFO("Closed serial port " << m_strPortName);
			m_hPort = 0;
		}
		else
		{
			handleError("closing serial port");
		}
	}
	return !isOpen();
}


bool SerialPort::setBaudrate(DWORD baudRate)
{
	bool success = false;

	if (isOpen())
	{
		DCB dcb = { 0 };
		dcb.DCBlength = sizeof(DCB);

		// read previous port state
		if (::GetCommState(m_hPort, &dcb))
		{
			// success > fill in new baudrate
			dcb.BaudRate = baudRate;
			dcb.ByteSize = 8;
			dcb.Parity   = NOPARITY;
			dcb.StopBits = ONESTOPBIT;
		}
		else
		{
			handleError("getting serial port state");
		}

		// set new baudrate
		if (::SetCommState(m_hPort, &dcb))
		{
			LOG_INFO("Set baudrate of serial port " << m_strPortName << " to " << baudRate);
			success = true;
		}
		else 
		{
			handleError("setting serial port state");
		}
	}

	return success;
}


DWORD SerialPort::getTimeout() const
{
	DWORD timeout = 0;

	if (isOpen())
	{
		COMMTIMEOUTS timeouts = { 0 };

		// read previous timeouts
		if (::GetCommTimeouts(m_hPort, &timeouts))
		{
			// success > read timeout
			timeout = timeouts.ReadTotalTimeoutConstant;
		}
		else
		{
			handleError("getting serial port timeouts");
		}
	}

	return timeout;
}


bool SerialPort::setTimeout(DWORD timeout)
{
	bool success = false;

	if (isOpen())
	{
		COMMTIMEOUTS timeouts = { 0 };

		// read previous timeouts
		if (::GetCommTimeouts(m_hPort, &timeouts))
		{
			// success > change timeout
			timeouts.ReadTotalTimeoutConstant = timeout;
		}
		else
		{
			handleError("getting serial port timeouts");
		}

		// set new timeout values
		if (::SetCommTimeouts(m_hPort, &timeouts))
		{
			// LOG_INFO("Set timeout of serial port " << m_strPortName << " to " << timeout);
			success = true;
		}
		else
		{
			handleError("setting serial port timeouts");
		}
	}

	return success;
}


DWORD SerialPort::send(const void* pBuffer, DWORD nBytesToSend) const
{
	DWORD nBytesSent= 0;
	::WriteFile(m_hPort, pBuffer, nBytesToSend, &nBytesSent, NULL);
	return nBytesSent;
}


DWORD SerialPort::receive(void* pBuffer, DWORD nBytesToReceive) const
{
	DWORD nBytesReceived = 0;
	::ReadFile(m_hPort, pBuffer, nBytesToReceive, &nBytesReceived, NULL);
	return nBytesReceived;
}


SerialPort::~SerialPort()
{
	if (isOpen())
	{
		close();
	}
}


void SerialPort::handleError(const char* strFunction) const
{
	LPVOID lpMsgBuf;
	DWORD  error = GetLastError();

	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL
	);

	std::wstring strMsgW((const wchar_t*) lpMsgBuf);
	std::string  strMsg(strMsgW.begin(), strMsgW.end());
	LOG_ERROR("Error while " << strFunction << ": " << strMsg);

	LocalFree(lpMsgBuf);
}

