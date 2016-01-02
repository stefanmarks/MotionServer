#include <iostream>
#include <sstream>

/**
 * Macros for logging information, warnings, and errors.
 *
 * The intermediate stringstream reduces the chances of threads interfering with each other's output.
 */
#define LOG_CLASS  "Global"
#define LOG_INFO(x)    { std::stringstream strm; strm << "I (" LOG_CLASS "):" << x << std::endl; std::cout << strm.str(); }
#define LOG_WARNING(x) { std::stringstream strm; strm << "W (" LOG_CLASS "):" << x << std::endl; std::cerr << strm.str(); }
#define LOG_ERROR(x)   { std::stringstream strm; strm << "E (" LOG_CLASS "):" << x << std::endl; std::cerr << strm.str(); }


#include "NatNetTypes.h"

/**
 * Prints a section of memory.
 */
void printMemory(std::ostream& refOutput, const void* pBuf, size_t length);


/**
 * Prints the model definition into an output stream. 
 */
void printModelDefinitions(std::ostream& refOutput, sDataDescriptions& refData);


/**
* Prints the current frame information into an output stream.
*/
void printFrameOfData(std::ostream& refOutput, sFrameOfMocapData& refData);

