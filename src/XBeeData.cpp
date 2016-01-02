#include "XBeeData.h"
#include "XBeePacket.h"



uint8_t* XBeeReadBuffer::prepareBuffer(uint16_t length)
{
	// prepare receive buffer: data length +4: +1x delimiter, +2x length bytes, +1 checksum
	m_buffer.reserve(length + 4);
	m_buffer.clear();
	m_read = m_buffer.cbegin();

	// reconstruct the header
	m_buffer.push_back(XBeePacket::START_DELIMITER);
	m_buffer.push_back((uint8_t) ((length >> 8) & 0xFF));
	m_buffer.push_back((uint8_t) ((length     ) & 0xFF));

	// fill up with 0 to create buffer space
	m_buffer.insert(m_buffer.end(), length + 1, 0);
	
	// return buffer position
	return m_buffer.data();
}


void XBeeReadBuffer::copy(const XBeeReadBuffer& refBuffer, size_t startPos, size_t endPos)
{
	m_buffer.clear();
	m_buffer.assign(
		refBuffer.m_buffer.cbegin() + startPos,
		refBuffer.m_buffer.cbegin() + endPos);
	m_read = m_buffer.cbegin();
}


uint8_t XBeeReadBuffer::getByteAt(size_t pos) const
{
	m_read = m_buffer.cbegin() + pos;
	return getNextByte();
}


uint8_t XBeeReadBuffer::getNextByte() const
{
	uint8_t result = *m_read; m_read++;
	return result;
}


uint16_t XBeeReadBuffer::getUInt16At(size_t pos) const
{
	// interpret the buffer as a 16 bit int (big endian)
	m_read = m_buffer.cbegin() + pos;
	return getNextUInt16();
}


uint16_t XBeeReadBuffer::getNextUInt16() const
{
	// interpret the buffer as a 16 bit int (big endian)
	uint16_t      result  = *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	return result;
}


uint32_t XBeeReadBuffer::getUInt32At(size_t pos) const
{
	// interpret the buffer as a 32 bit int (big endian)
	m_read = m_buffer.cbegin() + pos;
	return getNextUInt32();
}


uint32_t XBeeReadBuffer::getNextUInt32() const
{
	// interpret the buffer as a 32 bit int (big endian)
	uint32_t      result  = *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	return result;
}


uint64_t XBeeReadBuffer::getUInt64At(size_t pos) const
{
	// interpret the buffer as a 64 bit int (big endian)
	m_read = m_buffer.cbegin() + pos;
	return getNextUInt64();
}


uint64_t XBeeReadBuffer::getNextUInt64() const
{
	// interpret the buffer as a 64 bit int (big endian)
	uint64_t      result  = *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	result <<= 8; result |= *m_read; m_read++;
	return result;
}


std::string XBeeReadBuffer::getStringAt(size_t pos, size_t len) const
{
	m_read = m_buffer.cbegin() + pos;
	return getNextString(len);
}


std::string XBeeReadBuffer::getNextString(size_t len) const
{
	if (len == 0)
	{
		// maximum length: remainder of buffer
		len = m_buffer.cend() - m_read;
	}
	std::string str;
	while (len > 0)
	{
		if (*m_read != '\0')
		{
			str.append(1, *m_read);
			len--;
		}
		else
		{
			// found terminating '\0' > stop here
			len = 0;
		}
		m_read++;
	}
	return str;
}


size_t XBeeReadBuffer::size() const
{
	return m_buffer.size();
}


uint8_t XBeeReadBuffer::calculateChecksum() const
{
	uint8_t checksum = 0;
	for (size_t i = 3; i < m_buffer.size(); i++)
	{
		checksum += m_buffer[i];
	}
	return checksum;
}


const void* XBeeReadBuffer::data() const
{
	return m_buffer.data();
}





void XBeeWriteBuffer::clear()
{
	m_buffer.clear();
}


void XBeeWriteBuffer::setByteAt(size_t pos, uint8_t value)
{
	m_buffer[pos] = value;
}


void XBeeWriteBuffer::addByte(uint8_t value)
{
	m_buffer.push_back(value); 
}


void XBeeWriteBuffer::setUInt16At(size_t pos, uint16_t value)
{
	m_buffer[pos] = (uint8_t)((value >> 8) & 0xFF); pos++;
	m_buffer[pos] = (uint8_t)((value >> 0) & 0xFF);
}


void XBeeWriteBuffer::addUInt16(uint16_t value)
{
	m_buffer.push_back((uint8_t) ((value >> 8) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 0) & 0xFF));
}


void XBeeWriteBuffer::addUInt32(uint32_t value)
{
	m_buffer.push_back((uint8_t) ((value >> 24) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 16) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >>  8) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >>  0) & 0xFF));
}


void XBeeWriteBuffer::addUInt64(uint64_t value)
{
	m_buffer.push_back((uint8_t) ((value >> 56) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 48) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 40) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 32) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 24) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >> 16) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >>  8) & 0xFF));
	m_buffer.push_back((uint8_t) ((value >>  0) & 0xFF));
}


void XBeeWriteBuffer::addString(const std::string& value, size_t maxLen)
{
	if (maxLen == 0)
	{
		maxLen = value.size();
	}
	size_t pos = 0;
	while ( pos < maxLen )
	{
		m_buffer.push_back(value.at(pos)); 
		pos++;
	}
}


size_t XBeeWriteBuffer::size() const
{
	return m_buffer.size();
}


uint8_t XBeeWriteBuffer::calculateChecksum() const
{
	uint8_t checksum = 0;
	for (size_t i = 3; i < m_buffer.size(); i++)
	{
		checksum += m_buffer[i];
	}
	return checksum;
}


const void* XBeeWriteBuffer::data() const
{
	return m_buffer.data();
}




