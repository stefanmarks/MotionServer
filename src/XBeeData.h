/**
* Class for an 8 bit data buffer for reading and writing of data.
*
* (C) 2014-2016 Stefan Marks
* Auckland University of Technology
*/

#pragma once

#include <memory>
#include <vector>


class XBeeReadBuffer 
{
public:

	/**
	 * Prepares the buffer for receiving into.
	 *
	 * @param length the length of the pure data bytes to receive
	 *
	 * @return pointer to the beginning of the buffer
	 */
	uint8_t* prepareBuffer(uint16_t length);

	/**
	 * Copies data from another buffer.
	 *
	 * @param refBuffer  the buffer to copy from
	 * @param startPos   the position to start copying from
	 * @param endPos     the position to end copying
	 */
	void copy(const XBeeReadBuffer& refBuffer, size_t startPos, size_t endPos);

	/**
	 * Function for extracting an 8 bit byte from the buffer.
	 *
	 * @param pos  the position from where to extract
	 *
	 * @return the 8 bit number
	 */
	uint8_t  getByteAt(size_t pos) const;
	uint8_t  getNextByte() const;
	uint16_t getUInt16At(size_t pos) const;
	uint16_t getNextUInt16() const;
	uint32_t getUInt32At(size_t pos) const;
	uint32_t getNextUInt32() const;
	uint64_t getUInt64At(size_t pos) const;
	uint64_t getNextUInt64() const;

	std::string getStringAt(size_t pos, size_t len = 0) const;
	std::string getNextString(size_t len = 0) const;

	size_t size() const;

	uint8_t calculateChecksum() const;
	
	const void* data() const;

protected:

	        std::vector<uint8_t>                 m_buffer;
	mutable std::vector<uint8_t>::const_iterator m_read;

};



class XBeeWriteBuffer
{
public:

	void clear();

	void setByteAt(size_t pos, uint8_t value);
	void addByte(uint8_t value);
	void setUInt16At(size_t pos, uint16_t value);
	void addUInt16(uint16_t value);
	void addUInt32(uint32_t value);
	void addUInt64(uint64_t value);
	void addString(const std::string& value, size_t maxLen = 0);

	uint8_t calculateChecksum() const;

	size_t size() const;

	const void* data() const;


protected:

	std::vector<uint8_t>  m_buffer;

};


