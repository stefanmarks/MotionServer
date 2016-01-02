/**
* Class for an 8 bit data buffer for reading and writing of data.
*
* (C) 2014-2016 Stefan Marks
* Auckland University of Technology
*/

#pragma once

#include <memory>
#include <vector>


/******************************************************************************
 * Class for extracting numbers and strings from a byte buffer in Big Endian order.
 */
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
	 * Function for extracting a byte from the buffer.
	 *
	 * @param pos  the position from where to extract
	 *
	 * @return the extracted byte
	 */
	uint8_t getByteAt(size_t pos) const;

	/**
	 * Function for extracting the next byte from the buffer.
	 * 
	 * @return the next byte
	 */
	uint8_t getNextByte() const;

	/**
	 * Function for extracting a 16 bit integer number from a specific position in the buffer.
	 *
	 * @param pos  the position from where to extract
	 *
	 * @return the 16 bit number
	 */
	uint16_t getUInt16At(size_t pos) const;

	/**
	 * Function for extracting the next 16 bit integer number from the buffer.
	 *
	 * @return the next 16 bit integer number
	 */
	uint16_t getNextUInt16() const;

	/**
	 * Function for extracting a 32 bit integer number from a specific position in the buffer.
	 *
	 * @param pos  the position from where to extract
	 *
	 * @return the 32 bit number
	 */
	uint32_t getUInt32At(size_t pos) const;
	
	/**
	 * Function for extracting the next 32 bit integer number from the buffer.
	 *
	 * @return the next 32 bit integer number
	 */
	uint32_t getNextUInt32() const;

	/**
	 * Function for extracting a 64 bit integer number from a specific position in the buffer.
	 *
	 * @param pos  the position from where to extract
	 *
	 * @return the 64 bit number
	 */
	uint64_t getUInt64At(size_t pos) const;
	
	/**
	 * Function for extracting the next 64 bit integer number from the buffer.
	 *
	 * @return the next 64 bit integer number
	 */
	uint64_t getNextUInt64() const;

	/**
	 * Function for extracting a string from a specific position in the buffer.
	 *
	 * @param pos  the position from where to extract
	 * @param len  the maximum number of characters to extract
	 *             (0: extract until the end of the buffer or encountering a '\0')
	 *
	 * @return the extracted string
	 */
	std::string getStringAt(size_t pos, size_t len = 0) const;

	/**
	 * Function for extracting the next string from the buffer.
	 *
	 * @param len  the maximum number of characters to extract 
	 *             (0: extract until the end of the buffer or encountering a '\0')
	 *
	 * @return the extracted string
	 */
	std::string getNextString(size_t len = 0) const;

	/**
	 * Gets the amount of bytes in the buffer.
	 *
	 * @return  the amount of bytes in the buffer 
	 */
	size_t size() const;

	/**
	 * Calculates the additive checksum over the XBee buffer content.
	 *
	 * @return  the sum of all bytes in the buffer starting at position 3
	 */
	uint8_t calculateChecksum() const;
	
	/**
	 * Gets the raw pointer to the buffer content.
	 *
	 * @return a pointer to the buffer
	 */
	const void* data() const;

protected:

	        std::vector<uint8_t>                 m_buffer;
	mutable std::vector<uint8_t>::const_iterator m_read;

};



/******************************************************************************
 * Class for adding numbers and strings to a byte buffer in Big Endian order.
 */
class XBeeWriteBuffer
{
public:

	/**
	 * Clears the buffer.
	 */
	void clear();
	
	/**
	 * Appends a byte to the buffer.
	 *
	 * @param value  the byte to append.
	 */
	void addByte(uint8_t value);

	/**
	 * Sets a 16 bit integer number in the buffer.
	 *
	 * @param pos    the position where to change the number
	 * @param value  the value to change
	 */
	void setUInt16At(size_t pos, uint16_t value);

	/**
	 * Appends a 16 bit integer number to the buffer.
	 *
	 * @param value  the 16 bit integer number to append.
	 */
	void addUInt16(uint16_t value);

	/**
	* Appends a 64 bit integer number to the buffer.
	*
	* @param value  the 64 bit integer number to append.
	*/
	void addUInt64(uint64_t value);

	/**
	 * Appends a string to the buffer.
	 *
	 * @param value   the string to append.
	 * @param maxLen  the maximum amount of characters to append (0: all)
	 */
	void addString(const std::string& value, size_t maxLen = 0);

	/**
	 * Gets the amount of bytes in the buffer.
	 *
	 * @return  the amount of bytes in the buffer
	 */
	size_t size() const;

	/**
	 * Calculates the additive checksum over the XBee buffer content.
	 *
	 * @return  the sum of all bytes in the buffer starting at position 3
	 */
	uint8_t calculateChecksum() const;

	/**
	 * Gets the raw pointer to the buffer content.
	 *
	 * @return a pointer to the buffer
	 */
	const void* data() const;

protected:

	std::vector<uint8_t>  m_buffer;

};


