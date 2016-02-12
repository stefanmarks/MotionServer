#include "MoCapFile.h"

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapFile"

#include <iostream>
#include <sstream>
#include <time.h>
//#include <type>



MoCapFileWriter::MoCapFileWriter(float framerate) :
	updateRate(framerate)
{
	headerWritten = false;
}


MoCapFileWriter::~MoCapFileWriter()
{
	// clean up
	closeFile();
}


bool MoCapFileWriter::writeSceneDescription(const MoCapData& refData)
{
	bool success = false;

	if (openFile())
	{
		// header
		writeTag("MotionServer Data File"); write(1); write(updateRate);  nextLine(); // 1: File version

		// description block intro and count
		writeTag("Descriptions"); write(refData.description.nDataDescriptions); nextLine();

		for (int dIdx = 0; dIdx < refData.description.nDataDescriptions; dIdx++)
		{
			write(dIdx);
			const sDataDescription& descr = refData.description.arrDataDescriptions[dIdx];
			switch (descr.type)
			{
				case Descriptor_MarkerSet:
					writeTag("MarkerSet");
					writeMarkerSetDescription(*descr.Data.MarkerSetDescription);
					break;

				case Descriptor_RigidBody:
					writeTag("RigidBody");
					writeRigidBodyDescription(*descr.Data.RigidBodyDescription);
					break;

				case Descriptor_Skeleton:
					writeTag("Skeleton");
					writeSkeletonDescription(*descr.Data.SkeletonDescription);
					break;

				case Descriptor_ForcePlate:
					writeTag("ForcePlate");
					writeForcePlateDescription(*descr.Data.ForcePlateDescription);
					break;
			}
			nextLine();
		}

		// prepare frame data block
		writeTag("Frames"); nextLine();

		success = true;
		headerWritten = true;
		LOG_INFO("Header written");
	}

	return success;
}


bool MoCapFileWriter::writeFrameData(const MoCapData& refData)
{
	bool success = false;

	if (headerWritten && output.is_open())
	{
		const sFrameOfMocapData& frame = refData.frame;

		// frame number and latency
		write(frame.iFrame);
		write(frame.fLatency);
		
		// markersets
		writeTag("MarkerSets"); write(frame.nMarkerSets);
		for (int mIdx = 0; mIdx < frame.nMarkerSets; mIdx++)
		{
			writeMarkerSetData(frame.MocapData[mIdx]);
		}

		// rigid bodies
		writeTag("RigidBodies"); write(frame.nRigidBodies);
		for (int rIdx = 0; rIdx < frame.nRigidBodies; rIdx++)
		{
			writeRigidBodyData(frame.RigidBodies[rIdx]);
		}

		// skeletons
		writeTag("Skeletons"); write(frame.nSkeletons);
		for (int sIdx = 0; sIdx < frame.nSkeletons; sIdx++)
		{
			writeSkeletonData(frame.Skeletons[sIdx]);
		}

		// force plates
		writeTag("ForcePlates"); write(frame.nForcePlates);
		for (int fIdx = 0; fIdx < frame.nForcePlates; fIdx++)
		{
			writeForcePlateData(frame.ForcePlates[fIdx]);
		}
		
		nextLine();
		success = output.good();
	}

	return success;
}


void MoCapFileWriter::writeMarkerSetDescription(const sMarkerSetDescription& descr)
{
	write(descr.szName); write(descr.nMarkers);
	for (int mIdx = 0; mIdx < descr.nMarkers; mIdx++)
	{
		write(descr.szMarkerNames[mIdx]);
	}
}


void MoCapFileWriter::writeRigidBodyDescription(const sRigidBodyDescription& descr)
{
	write(descr.ID);
	write(descr.szName);
	write(descr.parentID);
	write(descr.offsetx); write(descr.offsety); write(descr.offsetz);
}


void MoCapFileWriter::writeSkeletonDescription(const sSkeletonDescription& descr)
{
	write(descr.skeletonID);
	write(descr.szName);
	write(descr.nRigidBodies);
	for (int rIdx = 0; rIdx < descr.nRigidBodies; rIdx++)
	{
		writeRigidBodyDescription(descr.RigidBodies[rIdx]);
	}
}


void MoCapFileWriter::writeForcePlateDescription(const sForcePlateDescription& descr)
{
	write(descr.ID);
	write(descr.strSerialNo);
	write(descr.nChannels);
	for (int cIdx = 0; cIdx < descr.nChannels; cIdx++)
	{
		write(descr.szChannelNames[cIdx]);
	}
}


void MoCapFileWriter::writeMarkerSetData(const sMarkerSetData& data)
{
	write(data.nMarkers);
	for (int mIdx = 0; mIdx < data.nMarkers; mIdx++)
	{
		const MarkerData& m = data.Markers[mIdx];
		write(m[0]); write(m[1]); write(m[2]); // XYZ coordinate of marker
	}
}


void MoCapFileWriter::writeRigidBodyData(const sRigidBodyData& data)
{
	write(data.ID);
	write(data.x); write(data.y); write(data.z);
	write(data.qx); write(data.qy); write(data.qz); write(data.qw);
	write(data.MeanError);
	write(data.params);
}


void MoCapFileWriter::writeSkeletonData(const sSkeletonData& data)
{
	write(data.skeletonID);
	write(data.nRigidBodies);
	for (int rIdx = 0; rIdx < data.nRigidBodies; rIdx++)
	{
		writeRigidBodyData(data.RigidBodyData[rIdx]);
	}
}


void MoCapFileWriter::writeForcePlateData(const sForcePlateData& data)
{
	write(data.ID);
	write(data.nChannels);
	for (int cIdx = 0; cIdx < data.nChannels; cIdx++)
	{
		write(data.ChannelData[cIdx].nFrames);
		for (int vIdx = 0; vIdx < data.ChannelData[cIdx].nFrames; vIdx++)
		{
			write(data.ChannelData[cIdx].Values[vIdx]);
		}
	}
}


void MoCapFileWriter::writeDelimiter()
{
	if (!lineStarted)
	{
		output.write("\t", 1);
	}
	lineStarted = false;
}


void MoCapFileWriter::write(float fValue)
{
	writeDelimiter();
	int len = sprintf_s(czBuf, "%f", fValue);
	output.write(czBuf, len);
}


void MoCapFileWriter::write(int iValue)
{
	writeDelimiter();
	int len = sprintf_s(czBuf, "%d", iValue); 
	output.write(czBuf, len);
}


void MoCapFileWriter::write(const char* czString)
{
	writeDelimiter();
	// put strings in quotation marks to be safe
	output << '"' << czString << '"';
}


void MoCapFileWriter::writeTag(const char* czString)
{
	writeDelimiter();
	// don't put tags in quotation marks
	output.write(czString, strlen(czString));
}


void MoCapFileWriter::nextLine()
{
	output << '\n'; // std::endl;
	lineStarted = true;
}


bool MoCapFileWriter::openFile()
{
	closeFile();
	std::string filename = getTimestampFilename();
	output.open(filename, std::ios::out);
	
	headerWritten = false;
	lineStarted   = true;
	LOG_INFO("Output file '" << filename << "' opened.");

	return output.is_open();
}


bool MoCapFileWriter::closeFile()
{
	if (output.is_open())
	{
		output.close();
		LOG_INFO("Output file closed.");
	}
	return !output.is_open();
}


std::string MoCapFileWriter::getTimestampFilename()
{
	time_t tTime = time(NULL);
	tm     tTimestamp;
	gmtime_s(&tTimestamp, &tTime);
	char czFilename[256];
	strftime(czFilename, sizeof(czFilename), "MotionServer File %Y_%m_%d_%H_%M_%S.mot", &tTimestamp);
	std::string strFilename(czFilename);
	return strFilename;
}






MoCapFileReader::MoCapFileReader(const std::string& strFilename) :
	strFilename(strFilename)
{
	// nothing else to do yet
}


MoCapFileReader::~MoCapFileReader()
{

}


bool MoCapFileReader::initialise()
{
	input.open(strFilename, std::ios::in);

	return isActive();
}


bool MoCapFileReader::isActive()
{
	return input.is_open();
}


float MoCapFileReader::getUpdateRate()
{
	return updateRate;
}


bool MoCapFileReader::update()
{
	return false;
}


bool MoCapFileReader::getSceneDescription(MoCapData& refData)
{
	return false;
}


bool MoCapFileReader::getFrameData(MoCapData& refData)
{
	return false;
}


bool MoCapFileReader::processCommand(const std::string& strCommand)
{
	// no commands recognised
	return false;
}


bool MoCapFileReader::deinitialise()
{
	input.close();
	return !input.is_open();
}
