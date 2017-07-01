/**
 * Motion Capture system for reading files and a class for writing files.
 */

#pragma once

#include "MoCapSystem.h"
#include "SystemConfiguration.h"
#include "VectorMath.h"

#include <fstream>
#include <sstream>
#include <string>


/**
 * Interface for writing ints/floats/strings to a file.
 * The underlying implementation determines the format, e.g., text, binary.
 */
class IFileWriter
{
public:
	virtual bool open(const std::string& filename) = 0;
	virtual bool close() = 0;
	virtual bool isOK() = 0;
	virtual void writeInt(int iValue) = 0;
	virtual void writeFloat(float fValue) = 0;
	virtual void writeString(const char* czString) = 0;
	virtual void writeTag(const char* czString) = 0;
	virtual void nextLine() = 0;
};


/**
 * Class for writing MoCap data to a text file.
 */
class MoCapFileWriter 
{
public:

	/**
	 * Creates a MoCap data file writer.
	 *
	 * @param framerate  the frame rate of the data in Hz
	 */
	MoCapFileWriter(float framerate);

	/**
	 * Destroys the MoCap data file writer.
	 */
	~MoCapFileWriter();

public:

	/**
	 * Writes the scene description to the file.
	 * This only needs to happen once at the beginning. 
	 * If the function is called again, it closes any previously opened file
	 * and starts a new one.
	 *
	 * @param refData  the MoCap data to write
	 *
	 * @return <code>true</code> if the data was written successfully
	 */
	bool writeSceneDescription(const MoCapData& refData);

	/**
	 * Writes a single frame of data to the file.
	 *
	 * @param refData  the MoCap data to write
	 *
	 * @return <code>true</code> if the data was written successfully
	 */
	bool writeFrameData(const MoCapData& refData);

private:

	/**
	 * Opens a new data file with the current timestamp.
	 *
	 * @return <code>true</code> if the file was opened successfully
	 */
	bool openFile();

	/**
	 * Closes any currently opened file.
	 *
	 * @return <code>true</code> if the file was opened successfully
	 */
	bool closeFile();

	/**
	 * Creates a string with a timestamp filename in the format
	 * "MotionServer File YYYY_MM_DD_HH_MM.SS.mot".
	 *
	 *@return the timestamp filename
	 */
	std::string getTimestampFilename();

	void writeMarkerSetDescription( const sMarkerSetDescription&  descr);
	void writeRigidBodyDescription( const sRigidBodyDescription&  descr);
	void writeSkeletonDescription(  const sSkeletonDescription&   descr);
	void writeForcePlateDescription(const sForcePlateDescription& descr);

	void writeFrameDataColumnNames(const MoCapData& refData);

	void writeMarkerSetData( const sMarkerSetData&  data);
	void writeRigidBodyData( const sRigidBodyData&  data);
	void writeSkeletonData(  const sSkeletonData&   data);
	void writeForcePlateData(const sForcePlateData& data);

	void writeDelimiter();
	void write(int   iValue);
	void write(float fValue);
	void write(const char* czString);
	void writeTag(const char* czString);
	void writeColumnName(const char* czString1, const char* czString2 = NULL, const char* czString3 = NULL);
	void writeColumnNames(const char* czString1, const char* czString2, int count, ...);
	void nextLine();

private:

	float         updateRate;
	std::ofstream output;
	bool          fileHeaderWritten, columnHeaderWritten, lineStarted;
	int           lastFrame;
	char*         pBuf;
	int           bufSize;
	char*         pWrite;
	char          czStrBuf[256];
};



/**
 * Class for the file reader MoCap system configuration.
 */
class MoCapFileReaderConfiguration : public SystemConfiguration
{
public:
	MoCapFileReaderConfiguration();

	virtual bool handleParameter(int idx, const std::string& value);

public:

	std::string filename;
};


/**
 * Class for reading MoCap data from a text file and acting like a live MoCap system.
 */
class MoCapFileReader : public MoCapSystem
{
public:

	/**
	 * Creates a MoCap data file reader.
	 *
	 * @param configuration  the configuration for the file reader
	 */
	MoCapFileReader(MoCapFileReaderConfiguration configuration);

	/**
	 * Destroys the MoCap data file reader.
	 */
	~MoCapFileReader();

public:
	virtual bool  initialise();
	virtual bool  isActive();
	virtual float getUpdateRate();
	virtual bool  isRunning();
	virtual void  setRunning(bool running);
	virtual bool  update();
	virtual bool  getSceneDescription(MoCapData& refData);
	virtual bool  getFrameData(MoCapData& refData);
	virtual bool  processCommand(const std::string& strCommand);
	virtual bool  deinitialise();

	/**
	 * Sets the playback speed of the file (0...INF)
	 *
	 * @return  the current playback speed
	 */
	float getSpeed();

	/**
	 * Sets the playback speed of the file (0...INF).
	 *
	 * @param speed  the new playback speed
	 */
	void  setSpeed(float speed);

private:

	/**
	 * Reads the header of the MoCap file and determines things like the version and the framerate.
	 *
	 * @return <code>true</code> if the header was read successfully
	 */
	bool readHeader();

	void readMarkerSetDescription( sMarkerSetDescription&  descr, sMarkerSetData&  data);
	void readRigidBodyDescription( sRigidBodyDescription&  descr, sRigidBodyData&  data);
	void readSkeletonDescription(  sSkeletonDescription&   descr, sSkeletonData&   data);
	void readForcePlateDescription(sForcePlateDescription& descr, sForcePlateData& data);

	void readMarkerSetData( sMarkerSetData&  data);
	void readRigidBodyData( sRigidBodyData&  data);
	void readSkeletonData(  sSkeletonData&   data);
	void readForcePlateData(sForcePlateData& data);

	void        nextLine();
	void        rewindLine();
	void        skipDelimiter();
	int         readInt();
	int         readInt(int min, int max);
	float       readFloat();
	const char* readString();
	bool        readTag(const char* czString);

private:

	MoCapFileReaderConfiguration configuration;

	int            fileVersion;
	float          updateRate;

	std::ifstream  input;
	char*          pBuf;
	int            bufSize;
	const char*    pRead;
	char           czStrBuf[256];

	std::streampos posDescriptions, posFrames;
	bool           fileOK, headerOK;

	bool           isPlaying, isLooping;
	float          playbackSpeed;
};

