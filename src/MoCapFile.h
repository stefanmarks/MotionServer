/**
 * Motion Capture system for reading files and a class for writing files.
 */

#pragma once

#include "MoCapSystem.h"
#include "VectorMath.h"

#include <fstream>
#include <string>


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

	int           fileVersion;
	float         updateRate;
	std::string   strFilename;
	std::ifstream input;
	std::string   activeLine;
};

