/**
 * Motion Capture system for reading files and a class for writing files.
 */

#pragma once

#include "MoCapSystem.h"
#include "VectorMath.h"

#include <fstream>
#include <sstream>
#include <string>


///////////////////////////////////////////////////////////////////////////////
//
class IFileWriter
{
public:
	virtual bool open(const std::string& filename) = 0;
	virtual bool close() = 0;
	virtual bool isOK() = 0;
	virtual void write(int   iValue) = 0;
	virtual void write(float fValue) = 0;
	virtual void write(const char* czString) = 0;
	virtual void writeTag(const char* czString) = 0;
	virtual void nextLine() = 0;
};


///////////////////////////////////////////////////////////////////////////////
//
class MoCapFileWriter 
{
public:
	MoCapFileWriter(float framerate);
	~MoCapFileWriter();

public:
	bool writeSceneDescription(const MoCapData& refData);
	bool writeFrameData(const MoCapData& refData);

private:
	bool openFile();
	bool closeFile();
	std::string getTimestampFilename();

	void writeMarkerSetDescription( const sMarkerSetDescription&  descr);
	void writeRigidBodyDescription( const sRigidBodyDescription&  descr);
	void writeSkeletonDescription(  const sSkeletonDescription&   descr);
	void writeForcePlateDescription(const sForcePlateDescription& descr);

	void writeMarkerSetData( const sMarkerSetData&  data);
	void writeRigidBodyData( const sRigidBodyData&  data);
	void writeSkeletonData(  const sSkeletonData&   data);
	void writeForcePlateData(const sForcePlateData& data);

	void writeDelimiter();
	void write(int   iValue);
	void write(float fValue);
	void write(const char* czString);
	void writeTag(const char* czString);
	void nextLine();

private:

	float         updateRate;
	std::ofstream output;
	bool          headerWritten, lineStarted;
	int           lastFrame;
	char          czBuf[256];
};


class MoCapFileReader : public MoCapSystem
{
public:
	MoCapFileReader(const std::string& strFilename);
	~MoCapFileReader();

public:
	virtual bool  initialise();
	virtual bool  isActive();
	virtual float getUpdateRate();
	virtual bool  update();
	virtual bool  getSceneDescription(MoCapData& refData);
	virtual bool  getFrameData(MoCapData& refData);
	virtual bool  processCommand(const std::string& strCommand);
	virtual bool  deinitialise();

private:
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
	void        skipDelimiter();
	int         readInt();
	int         readInt(int min, int max);
	float       readFloat();
	const char* readString();
	bool        readTag(const char* czString);

private:

	int                fileVersion;
	float              updateRate;
	std::string        strFilename;
	std::ifstream      input;
	char*              pBuf;
	char               czBuf[256];
	const char*        pRead;
	int                bufSize;
	std::streampos     posDescriptions, posFrames;
	bool               fileOK, headerOK;
};

