#include "MoCapFile.h"

#include "Logging.h"
#undef   LOG_CLASS

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include <time.h>
#include <stdarg.h>


// tag names for file sections or identifiers
#define TAG_HEADER               "MotionServer Data File"

#define TAG_SECTION_DESCRIPTIONS "Descriptions"
#define TAG_SECTION_FRAMES       "Frames"

#define TAG_MARKERSET   "M"
#define TAG_RIGIDBODY   "R"
#define TAG_SKELETON    "S"
#define TAG_FORCEPLATE  "F"

#define limitArrayIdx(x, y) ((x > (y-1)) ? (y-1) : (x))


/******************************************************************************
 * MoCapFileWriter class
 */

#define  LOG_CLASS "MoCapFileWriter"

MoCapFileWriter::MoCapFileWriter(float framerate) :
	updateRate(framerate),
	fileHeaderWritten(false),
	columnHeaderWritten(false),
	lastFrame(-1),
	bufSize(65536) // should be a good start for a buffer size...
{
	pBuf   = new char[bufSize];
	pWrite = pBuf;
}


MoCapFileWriter::~MoCapFileWriter()
{
	// clean up
	closeFile();

	if (pBuf)
	{
		delete[] pBuf;
		pBuf = NULL;
	}
}


bool MoCapFileWriter::writeSceneDescription(const MoCapData& refData)
{
	bool success = false;

	if (openFile())
	{
		// header
		writeTag(TAG_HEADER); write(1); write(updateRate);  nextLine(); // 1: File version

		// description block intro and count
		writeTag(TAG_SECTION_DESCRIPTIONS); write(refData.description.nDataDescriptions); nextLine();

		for (int dIdx = 0; dIdx < refData.description.nDataDescriptions; dIdx++)
		{
			write(dIdx);
			const sDataDescription& descr = refData.description.arrDataDescriptions[dIdx];
			switch (descr.type)
			{
				case Descriptor_MarkerSet:
					writeTag(TAG_MARKERSET);
					writeMarkerSetDescription(*descr.Data.MarkerSetDescription);
					break;

				case Descriptor_RigidBody:
					writeTag(TAG_RIGIDBODY);
					writeRigidBodyDescription(*descr.Data.RigidBodyDescription);
					break;

				case Descriptor_Skeleton:
					writeTag(TAG_SKELETON);
					writeSkeletonDescription(*descr.Data.SkeletonDescription);
					break;

				case Descriptor_ForcePlate:
					writeTag(TAG_FORCEPLATE);
					writeForcePlateDescription(*descr.Data.ForcePlateDescription);
					break;
			}
			nextLine();
		}

		// prepare frame data block
		writeTag(TAG_SECTION_FRAMES); nextLine();

		success             = true;
		fileHeaderWritten   = true;
		columnHeaderWritten = false;
		lastFrame           = -1;
		LOG_INFO("Header written");
	}

	return success;
}


bool MoCapFileWriter::writeFrameData(const MoCapData& refData)
{
	bool success = false;
	const sFrameOfMocapData& frame = refData.frame;

	if ((lastFrame >= 0) && (frame.iFrame <= lastFrame))
	{
		// frame# repeat > skip (but don't signal as error)
		success = true;
	}
	else if (fileHeaderWritten && output.is_open())
	{
		// do we still need to write the column header?
		// (and don't move this to writeSceneDescription, because the data structure is probably not complete there,
		//  -> you need to wait for the first data frame)
		if (!columnHeaderWritten)
		{
			// write line with column names (e.g., for reading into a spreadsheet)
			writeFrameDataColumnNames(refData); nextLine();
			columnHeaderWritten = true;
		}

		// frame number and latency
		write(frame.iFrame);
		write(frame.fLatency);
		
		// markersets
		writeTag(TAG_MARKERSET);
		write(frame.nMarkerSets);
		for (int mIdx = 0; mIdx < frame.nMarkerSets; mIdx++)
		{
			writeMarkerSetData(frame.MocapData[mIdx]);
		}

		// rigid bodies
		writeTag(TAG_RIGIDBODY);
		write(frame.nRigidBodies);
		for (int rIdx = 0; rIdx < frame.nRigidBodies; rIdx++)
		{
			writeRigidBodyData(frame.RigidBodies[rIdx]);
		}

		// skeletons
		writeTag(TAG_SKELETON);
		write(frame.nSkeletons);
		for (int sIdx = 0; sIdx < frame.nSkeletons; sIdx++)
		{
			writeSkeletonData(frame.Skeletons[sIdx]);
		}

		// force plates
		writeTag(TAG_FORCEPLATE); 
		write(frame.nForcePlates);
		for (int fIdx = 0; fIdx < frame.nForcePlates; fIdx++)
		{
			writeForcePlateData(frame.ForcePlates[fIdx]);
		}
		
		nextLine();
		success = output.good();
		lastFrame = frame.iFrame;
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


void MoCapFileWriter::writeFrameDataColumnNames(const MoCapData& refData)
{
	writeColumnName("#frame"); // '#': when reading, consider this line a comment
	writeColumnName("latency");
	
	// markersets
	writeColumnName("markersetTag");
	writeColumnName("markersetCount");
	for (int msIdx = 0; msIdx < refData.frame.nMarkerSets; msIdx++)
	{
		const sMarkerSetData&  data  = refData.frame.MocapData[msIdx];
		// need to get description for individual marker names
		sMarkerSetDescription* descr = refData.findMarkerSetDescription(data);
		
		// construct column prefix from markerset tag followed by "." and markerset name
		char czMarkersetName[MAX_NAMELENGTH + 2];
		sprintf_s(czMarkersetName, "%s.%s", TAG_MARKERSET, data.szName);

		// write column names
		writeColumnName(czMarkersetName, "markerCount");
		for (int mIdx = 0; mIdx < data.nMarkers; mIdx++)
		{ 
			char czMarkerName[MAX_NAMELENGTH];
			if (descr != NULL)
			{
				strcpy_s(czMarkerName, descr->szMarkerNames[mIdx]);
			}
			else
			{
				// something went wrong -> use generic marker name
				sprintf_s(czMarkerName, "M%d", mIdx);
			}
			writeColumnNames(czMarkersetName, czMarkerName, 3, "x", "y", "z");
		}
	}

	// rigid bodies
	writeColumnName("rigidbodyTag");
	writeColumnName("rigidbodyCount");
	for (int rbIdx = 0; rbIdx < refData.frame.nRigidBodies; rbIdx++)
	{
		const sRigidBodyData&  data = refData.frame.RigidBodies[rbIdx];
		
		// need to get description for individual rigid bodies
		sRigidBodyDescription* descr = refData.findRigidBodyDescription(data);
		
		// construct rigid body name for colum header with tag followed by "."
		char czRigidBodyName[MAX_NAMELENGTH + 2];
		if (descr != NULL)
		{
			sprintf_s(czRigidBodyName, "%s.%s", TAG_RIGIDBODY, descr->szName);
		}
		else
		{
			// something went wrong -> use generic name
			sprintf_s(czRigidBodyName, "%s.%d", TAG_RIGIDBODY, rbIdx);
		}
		
		writeColumnNames(czRigidBodyName, NULL, 10, "id", "x", "y", "z", "qx", "qy", "qz", "qw", "meanError", "params");
	}

	// skeletons
	writeColumnName("skeletonTag");
	writeColumnName("skeletonCount");
	for (int skIdx = 0; skIdx < refData.frame.nSkeletons; skIdx++)
	{
		const sSkeletonData&  data = refData.frame.Skeletons[skIdx];

		// need to get description for individual skeletons
		sSkeletonDescription* descr = refData.findSkeletonDescription(data);

		// construct rigid body name for colum header with tag followed by "."
		char czSkeletonName[MAX_NAMELENGTH + 2];
		if (descr != NULL)
		{
			sprintf_s(czSkeletonName, "%s.%s", TAG_SKELETON, descr->szName);
		}
		else
		{
			// something went wrong -> use generic name
			sprintf_s(czSkeletonName, "%s.%d", TAG_SKELETON, skIdx);
		}

		writeColumnName(czSkeletonName, "id");
		writeColumnName(czSkeletonName, "boneCount");

		for (int rbIdx = 0; rbIdx < data.nRigidBodies; rbIdx++)
		{
			const sRigidBodyData&  dataBone = data.RigidBodyData[rbIdx];
			// construct bone/rigid body name for colum header 
			char czRigidBodyName[MAX_NAMELENGTH];
			if (descr != NULL)
			{
				sprintf_s(czRigidBodyName, "%s", descr->RigidBodies[rbIdx].szName);
			}
			else
			{
				// something went wrong -> use generic name
				sprintf_s(czRigidBodyName, "B%d", rbIdx);
			}

			writeColumnNames(czSkeletonName, czRigidBodyName, 10, "id", "x", "y", "z", "qx", "qy", "qz", "qw", "length", "params");
		}
	}

	// force plates
	writeColumnName("forceplateTag");
	writeColumnName("forceplateCount");
	for (int fpIdx = 0; fpIdx < refData.frame.nForcePlates; fpIdx++)
	{
		const sForcePlateData&  data = refData.frame.ForcePlates[fpIdx];

		// need to get description for individual force plates
		sForcePlateDescription* descr = refData.findForcePlateDescription(data);

		// construct force plate name for colum header with tag followed by "."
		char czForcePlateName[MAX_NAMELENGTH + 2];
		if (descr != NULL)
		{
			sprintf_s(czForcePlateName, "%s.%s", TAG_FORCEPLATE, descr->strSerialNo);
		}
		else
		{
			// something went wrong -> use generic name
			sprintf_s(czForcePlateName, "%s.%d", TAG_FORCEPLATE, fpIdx);
		}

		writeColumnName(czForcePlateName, "id");
		writeColumnName(czForcePlateName, "channelCount");

		for (int chIdx = 0; chIdx < data.nChannels; chIdx++)
		{
			const sAnalogChannelData&  dataChannel = data.ChannelData[chIdx];

			// construct channel name for colum header 
			char czChannelName[MAX_NAMELENGTH];
			if (descr != NULL)
			{
				sprintf_s(czChannelName, "%s", descr->szChannelNames[chIdx]);
			}
			else
			{
				// something went wrong -> use generic name
				sprintf_s(czChannelName, "C%d", chIdx);
			}

			writeColumnName(czForcePlateName, czChannelName);
		}
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
		// file stores only one frame per tick, 
		write(data.ChannelData[cIdx].Values[0]);
	}
}


void MoCapFileWriter::writeDelimiter()
{
	if (!lineStarted)
	{
		*pWrite++ = '\t';
	}
	lineStarted = false;
}


void MoCapFileWriter::write(float fValue)
{
	writeDelimiter();
	size_t bufRemaining = bufSize - (pWrite - pBuf); // how much buffer is left
	int len = sprintf_s(pWrite, bufRemaining, "%f", fValue);
	while ((len > 1) && (pWrite[len - 1] == '0')) len--; // cut trailing zeroes
	if    ((len > 1) && (pWrite[len - 1] == '.')) len--; // and if possible even the decimal dot
	pWrite += len;
}


void MoCapFileWriter::write(int iValue)
{
	writeDelimiter();
	size_t bufRemaining = bufSize - (pWrite - pBuf); // how much buffer is left
	pWrite += sprintf_s(pWrite, bufRemaining, "%d", iValue);
}


void MoCapFileWriter::write(const char* czString)
{
	writeDelimiter();
	// put strings in quotation marks to be safe
	*pWrite++ = '"';
	// copy string content
	int idx = 0;
	while (czString[idx] != '\0')
	{
		*pWrite++ = czString[idx++];
	}
	// close quotation mark
	*pWrite++ = '"';
}


void MoCapFileWriter::writeTag(const char* czString)
{
	writeDelimiter();
	// don't put tags in quotation marks
	int idx = 0;
	while (czString[idx] != '\0')
	{
		*pWrite++ = czString[idx++];
	}
}


void MoCapFileWriter::writeColumnName(const char* czString1, const char* czString2, const char* czString3)
{
	writeDelimiter();

	// write first part
	int idx = 0;
	while (czString1[idx] != '\0')
	{
		*pWrite++ = czString1[idx++];
	}

	// is there a second part to it?
	if (czString2 != NULL)
	{
		*pWrite++ = '.'; // separate with period
		idx = 0;
		while (czString2[idx] != '\0')
		{
			*pWrite++ = czString2[idx++];
		}
	}

	// is there a third part to it?
	if (czString3 != NULL)
	{
		*pWrite++ = '.'; // separate with period
		idx = 0;
		while (czString3[idx] != '\0')
		{
			*pWrite++ = czString3[idx++];
		}
	}
}


void MoCapFileWriter::writeColumnNames(const char* czString1, const char* czString2, int count, ...)
{
	va_list arguments;
	va_start(arguments, count);
	const char* czArg;
	
	// iterate through variable arguments
	while (count > 0)
	{
		czArg = va_arg(arguments, const char*);
		writeColumnName(czString1, czString2, czArg);
		count--;
	};

	va_end(arguments);
}


void MoCapFileWriter::nextLine()
{
	// close output string
	*pWrite++ = '\n';
	// write to disk
	output.write(pBuf, pWrite - pBuf);
	// reset write pointer
	pWrite = pBuf;
	lineStarted = true;
}


bool MoCapFileWriter::openFile()
{
	closeFile();
	std::string filename = getTimestampFilename();
	output.open(filename, std::ios::out);
	
	fileHeaderWritten = false;
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
	localtime_s(&tTimestamp, &tTime);
	char czFilename[256];
	strftime(czFilename, sizeof(czFilename), "MotionServer File %Y_%m_%d_%H_%M_%S.mot", &tTimestamp);
	std::string strFilename(czFilename);
	return strFilename;
}




/******************************************************************************
 * MoCapFileReaderConfiguration class
 */

#undef  LOG_CLASS
#define LOG_CLASS "MoCapFileReader"

#define MIN_PLAYBACK_SPEED 0.01f
#define MAX_PLAYBACK_SPEED 10.0f


MoCapFileReaderConfiguration::MoCapFileReaderConfiguration() :
	SystemConfiguration("MoCap File Reader"),
	filename("")
{
	addParameter("-readFile", "<MOT file name>", "Load a MoCap recording file");
}


bool MoCapFileReaderConfiguration::handleParameter(int idx, const std::string& value)
{
	bool success = true;
	switch (idx)
	{
		case 0:
			filename = value;
			break;

		default:
			success = false;
			break;
	}
	return success;
}


/******************************************************************************
 * MoCapFileReader class
 */

MoCapFileReader::MoCapFileReader(MoCapFileReaderConfiguration configuration) :
	configuration(configuration),
	updateRate(0),
	pBuf(NULL), pRead(NULL),
	bufSize(65536), // should be a good start for a buffer size...
	running(true),
	looping(true),
	playbackSpeed(1.0f)
{
	pBuf  = new char[bufSize];
	pRead = pBuf;
}


MoCapFileReader::~MoCapFileReader()
{
	deinitialise();
}


bool MoCapFileReader::initialise()
{
	bool success = false;
	
	input.open(configuration.filename, std::ios::in);
	if (input.is_open())
	{
		posDescriptions = -1;
		posFrames       = -1;
		success = readHeader();
		fileOK  = success;
		headerOK = false;
	}

	return success;
}


bool MoCapFileReader::isActive()
{
	return input.is_open();
}


float MoCapFileReader::getUpdateRate()
{
	return updateRate * playbackSpeed;
}


bool MoCapFileReader::isRunning()
{
	return running;
}


void MoCapFileReader::setRunning(bool running)
{
	this->running = running;
}


bool MoCapFileReader::update()
{
	if (fileOK && headerOK)
	{
		signalNewFrame();
	}
	return true;
}


bool MoCapFileReader::getSceneDescription(MoCapData& refData)
{
	bool success = false;
	if (posDescriptions > 0)
	{
		// jump to file position for descriptions
		input.seekg(posDescriptions);

		nextLine();
		if (readTag(TAG_SECTION_DESCRIPTIONS))
		{
			int nDataDescriptions = readInt(); nextLine();

			refData.description.nDataDescriptions = 0;
			success = true; // now be optimistic at first
			for (int dIdx = 0; dIdx < nDataDescriptions && success; dIdx++)
			{
				sDataDescription&  refDescr = refData.description.arrDataDescriptions[dIdx];

				int index = readInt(); // description index
				if (index != dIdx)
				{
					LOG_WARNING("Wrong index " << index << " for descriptor " << dIdx);
				}

				// what descriptor type is it > parse accordingly
				const char* czType = readString();
				if ( _stricmp(czType, TAG_MARKERSET) == 0)
				{
					sMarkerSetDescription* pDescr   = new sMarkerSetDescription;
					sMarkerSetData&        refMData = refData.frame.MocapData[refData.frame.nMarkerSets];
					refData.frame.nMarkerSets++;
					readMarkerSetDescription(*pDescr, refMData);
					refDescr.Data.MarkerSetDescription = pDescr;
					refDescr.type = Descriptor_MarkerSet;
				}
				else if (_stricmp(czType, TAG_RIGIDBODY) == 0)
				{
					sRigidBodyDescription* pDescr   = new sRigidBodyDescription;
					sRigidBodyData&        refRData = refData.frame.RigidBodies[refData.frame.nRigidBodies];
					refData.frame.nRigidBodies++;
					readRigidBodyDescription(*pDescr, refRData);
					refDescr.Data.RigidBodyDescription = pDescr;
					refDescr.type = Descriptor_RigidBody;
				}
				else if (_stricmp(czType, TAG_SKELETON) == 0)
				{
					sSkeletonDescription* pDescr   = new sSkeletonDescription;
					sSkeletonData&        refSData = refData.frame.Skeletons[refData.frame.nSkeletons];
					refData.frame.nSkeletons++;
					readSkeletonDescription(*pDescr, refSData);
					refDescr.Data.SkeletonDescription = pDescr;
					refDescr.type = Descriptor_Skeleton;
				}
				else if (_stricmp(czType, TAG_FORCEPLATE) == 0)
				{
					sForcePlateDescription* pDescr   = new sForcePlateDescription;
					sForcePlateData&        refFData = refData.frame.ForcePlates[refData.frame.nForcePlates];
					refData.frame.nForcePlates++;
					readForcePlateDescription(*pDescr, refFData);
					refDescr.Data.ForcePlateDescription = pDescr;
					refDescr.type = Descriptor_ForcePlate;
				}
				else
				{
					LOG_WARNING("Error while reading description #" << refData.description.nDataDescriptions);
					success = false;
				}

				nextLine();
				refData.description.nDataDescriptions++;
			}
			
			if (success)
			{
				headerOK = true;
				refData.frame.nOtherMarkers    = 0;
				refData.frame.OtherMarkers     = NULL;
				refData.frame.nLabeledMarkers  = 0;
				refData.frame.Timecode         = 0;
				refData.frame.TimecodeSubframe = 0;
			}
			else
			{
				refData.description.nDataDescriptions = 0;
			}
		}
	}
	return success;
}


bool MoCapFileReader::getFrameData(MoCapData& refData)
{
	bool success = fileOK;

	// have we found the frame block begin?
	if (posFrames < 0)
	{
		// no > look for it
		while (input.good() && !readTag(TAG_SECTION_FRAMES))
		{
			nextLine();
		}
		// found frame data header?
		if (input.good())
		{
			// mark position
			posFrames = input.tellg();
			nextLine();
		}
		else
		{
			LOG_WARNING("Could not find Frame data block header");
			success = false;
		}
	}
	else if (!input.good())
	{
		if (looping)
		{
			// end of file reached > clear failbit and loop to beginning
			input.clear();
			input.seekg(posFrames);
			nextLine();
			LOG_INFO("End of data reached > Looping");
		}
		else
		{
			// not looping, pause here
			running = false;
			LOG_INFO("End of data reached > Stopping");
		}
	}
	else
	{
		if (running)
		{
			nextLine();
		}
		else
		{
			// get "stuck" on that current line
			rewindLine();
		}
	}

	if (success && input.good())
	{
		sFrameOfMocapData& frame = refData.frame;

		// frame number and latency
		frame.iFrame   = readInt();
		frame.fLatency = readFloat();

		// markersets
		if (readTag(TAG_MARKERSET) && (readInt() == frame.nMarkerSets))
		{
			for (int mIdx = 0; mIdx < frame.nMarkerSets; mIdx++)
			{
				readMarkerSetData(frame.MocapData[mIdx]);
			}
		}
		else
		{
			LOG_WARNING("Error in markerset data for frame " << frame.iFrame);
			success = false;
		}

		// rigid bodies
		if (readTag(TAG_RIGIDBODY) && (readInt() == frame.nRigidBodies))
		{
			for (int rIdx = 0; rIdx < frame.nRigidBodies; rIdx++)
			{
				readRigidBodyData(frame.RigidBodies[rIdx]);
			}
		}
		else
		{
			LOG_WARNING("Error in rigid body data for frame " << frame.iFrame);
			success = false;
		}

		// skeletons
		if (readTag(TAG_SKELETON) && (readInt() == frame.nSkeletons))
		{
			for (int sIdx = 0; sIdx < frame.nSkeletons; sIdx++)
			{
				readSkeletonData(frame.Skeletons[sIdx]);
			}
		}
		else
		{
			LOG_WARNING("Error in skeleton data for frame " << frame.iFrame);
			success = false;
		}

		// force plates
		if (readTag(TAG_FORCEPLATE) && (readInt() == frame.nForcePlates))
		{
			for (int fIdx = 0; fIdx < frame.nForcePlates; fIdx++)
			{
				readForcePlateData(frame.ForcePlates[fIdx]);
			}
		}
		else
		{
			LOG_WARNING("Error in force plate data for frame " << frame.iFrame);
			success = false;
		}
	}

	fileOK &= success; // one error is enough

	return success;
}


bool MoCapFileReader::processCommand(const std::string& strCommand)
{
	bool processed = false;
	
	// convert commandto lowercase
	std::string strCmdLowerCase;
	std::transform(strCommand.begin(), strCommand.end(), std::back_inserter(strCmdLowerCase), ::tolower);
	
	if (strCmdLowerCase.find("setspeed") == 0)
	{
		size_t paramPos = strCmdLowerCase.find_first_of(" ");
		if (paramPos > 0)
		{
			float speed = (float) atof(strCmdLowerCase.c_str() + paramPos);
			setSpeed(speed);
			processed = true;
		}
	}

	return processed;
}


bool MoCapFileReader::deinitialise()
{
	// close file
	if (input.is_open())
	{
		input.close();
		LOG_INFO("MoCap data file '" << configuration.filename << "' closed");
	}

	// release read buffer
	if (pBuf)
	{
		delete[] pBuf;
		pBuf = NULL;
	}

	return true;
}


float MoCapFileReader::getSpeed()
{
	return playbackSpeed;
}


void MoCapFileReader::setSpeed(float speed)
{
	if (speed < MIN_PLAYBACK_SPEED) { speed = MIN_PLAYBACK_SPEED; }
	if (speed > MAX_PLAYBACK_SPEED) { speed = MAX_PLAYBACK_SPEED; }
	playbackSpeed = speed;
	LOG_INFO("Playback Speed changed to " << playbackSpeed);
}


bool MoCapFileReader::readHeader()
{
	bool success = false;
	// check header
	input.seekg(0);
	nextLine();
	if (readTag(TAG_HEADER))
	{
		// read version and update rate
		fileVersion     = readInt(); 
		updateRate      = readFloat();
		posDescriptions = input.tellg();

		// next should be the definitions
		nextLine();
		if (readTag(TAG_SECTION_DESCRIPTIONS))
		{
			int nDescriptions = readInt();
			LOG_INFO("Opened MoCap data file '" << configuration.filename << "' "
				<< "(v" << fileVersion 
				<< ", Sample Rate: " << updateRate << "Hz"
				<< ", Descriptions: " << nDescriptions << ")");

			success = (fileVersion == 1);
		}
	}
	else
	{
		LOG_WARNING("File is not a valid MoCap data file");
	}
	return success;
}


void MoCapFileReader::readMarkerSetDescription(sMarkerSetDescription& descr, sMarkerSetData& data)
{
	strcpy_s(descr.szName, sizeof(descr.szName), readString());
	strcpy_s(data.szName, sizeof(data.szName), descr.szName);
	descr.nMarkers = readInt(0, MAX_MARKERS);
	descr.szMarkerNames = new char*[descr.nMarkers];
	for (int mIdx = 0; mIdx < descr.nMarkers; mIdx++)
	{
		descr.szMarkerNames[mIdx] = _strdup(readString());
	}

	data.nMarkers = descr.nMarkers;
	data.Markers  = new MarkerData[data.nMarkers];
}


void MoCapFileReader::readRigidBodyDescription(sRigidBodyDescription& descr, sRigidBodyData& data)
{
	descr.ID = readInt();
	strcpy_s(descr.szName, sizeof(descr.szName), readString());
	descr.parentID = readInt();
	descr.offsetx = readFloat(); descr.offsety = readFloat(); descr.offsetz = readFloat();

	data.ID          = descr.ID;
	data.nMarkers    = 0;
	data.Markers     = NULL;
	data.MarkerIDs   = NULL;
	data.MarkerSizes = NULL;
	data.MeanError   = 0;
}


void MoCapFileReader::readSkeletonDescription(sSkeletonDescription& descr, sSkeletonData& data)
{
	descr.skeletonID = readInt();
	strcpy_s(descr.szName, sizeof(descr.szName), readString());
	descr.nRigidBodies = readInt(0, MAX_RIGIDBODIES);

	data.skeletonID    = descr.skeletonID;
	data.nRigidBodies  = descr.nRigidBodies;
	data.RigidBodyData = new sRigidBodyData[data.nRigidBodies];

	for (int rIdx = 0; rIdx < descr.nRigidBodies; rIdx++)
	{
		readRigidBodyDescription(descr.RigidBodies[rIdx], data.RigidBodyData[rIdx]);
	}
}


void MoCapFileReader::readForcePlateDescription(sForcePlateDescription& descr, sForcePlateData& data)
{
	descr.ID = readInt();
	strcpy_s(descr.strSerialNo, sizeof(descr.strSerialNo), readString());
	descr.nChannels = readInt(0, MAX_ANALOG_CHANNELS);
	for (int cIdx = 0; cIdx < descr.nChannels; cIdx++)
	{
		strcpy_s(descr.szChannelNames[cIdx], sizeof(descr.szChannelNames[cIdx]), readString());
	}

	data.ID        = descr.ID;
	data.nChannels = descr.nChannels;
}


void MoCapFileReader::readMarkerSetData(sMarkerSetData& data)
{
	int nMarkers = readInt(); 
	if (nMarkers != data.nMarkers)
	{
		LOG_WARNING("Marker count mismatch in frame data (" << nMarkers << " != " << data.nMarkers << ")");
	}
	for (int mIdx = 0; mIdx < nMarkers; mIdx++)
	{
		MarkerData& m = data.Markers[limitArrayIdx(mIdx, data.nMarkers)];
		m[0] = readFloat(); m[1] = readFloat(); m[2] = readFloat(); // XYZ coordinate of marker
	}
}


void MoCapFileReader::readRigidBodyData(sRigidBodyData& data)
{
	int id = readInt(); 
	if (id != data.ID)
	{
		LOG_WARNING("Rigid Body ID mismatch in frame data (" << id << " != " << data.ID << ")");
	}
	data.x  = readFloat(); data.y  = readFloat(); data.z  = readFloat();
	data.qx = readFloat(); data.qy = readFloat(); data.qz = readFloat(); data.qw = readFloat();
	data.MeanError = readFloat();
	data.params    = readInt();
}


void MoCapFileReader::readSkeletonData(sSkeletonData& data)
{
	int id = readInt();
	if (id != data.skeletonID)
	{
		LOG_WARNING("Rigid Body ID mismatch in frame data (" << id << " != " << data.skeletonID << ")");
	}
	int nRigidBodies = readInt();
	if (nRigidBodies != data.nRigidBodies)
	{
		LOG_WARNING("Rigid Body count mismatch in frame data (" << nRigidBodies << " != " << data.nRigidBodies << ")");
	}
	for (int rIdx = 0; rIdx < data.nRigidBodies; rIdx++)
	{
		readRigidBodyData(data.RigidBodyData[limitArrayIdx(rIdx, data.nRigidBodies)]);
	}
}


void MoCapFileReader::readForcePlateData(sForcePlateData& data)
{
	int id = readInt();
	if (id != data.ID)
	{
		LOG_WARNING("Force Plate ID mismatch in frame data (" << id << " != " << data.ID << ")");
	}
	int nChannels = readInt();
	if (nChannels != data.nChannels)
	{
		LOG_WARNING("Channel count mismatch in frame data (" << nChannels << " != " << data.nChannels << ")");
	}
	for (int cIdx = 0; cIdx < nChannels; cIdx++)
	{
		sAnalogChannelData& refChannel = data.ChannelData[limitArrayIdx(cIdx, data.nChannels)];
		refChannel.nFrames = 1; // file stores only one sample per tick
		refChannel.Values[0] = readFloat();
	}
}


void MoCapFileReader::nextLine()
{
	bool repeat = true;
	// remember where we are now
	std::streampos pos = input.tellg();
	while (repeat)
	{
		// read to next newline
		input.getline(pBuf, bufSize);
		// how much was read (including \n)
		std::streamsize readBytes = input.gcount() + 1;
		if (readBytes == bufSize)
		{
			// buffer seems to be too small > double the size
			delete[] pBuf;
			bufSize <<= 1;
			pBuf = new char[bufSize];
			LOG_INFO("Resized read buffer to " << bufSize << " bytes");
			// clear the fail bit, and read again from that position
			input.clear();
			input.seekg(pos, input.beg);
		}
		else
		{
			// done > get me out
			repeat = false;
		}

		// check if line is a comment
		if (pBuf[0] == '#')
		{
			repeat = true;
		}
	} 
	pRead = pBuf;
}


void MoCapFileReader::rewindLine()
{
	pRead = pBuf;
}


void MoCapFileReader::skipDelimiter()
{
	while (*pRead == '\t') { pRead++; }
}


int MoCapFileReader::readInt()
{
	return atoi(readString());
}


int MoCapFileReader::readInt(int min, int max)
{
	int value = readInt();
	if (value < min) { value = min; }
	if (value > max) { value = max; }
	return value;
}


float MoCapFileReader::readFloat()
{
	return (float) atof(readString());
}


const char* MoCapFileReader::readString()
{
	skipDelimiter();
	
	char* pStr = czStrBuf;

	// skip opening quotation mark
	if (*pRead == '"') { pRead++; }

	// do we need to cut quotation marks?
	while (*pRead != '"' && *pRead != '\t' && *pRead != '\0') 
	{ 
		*pStr++ = *pRead++;
	}

	// skip closing quotaion mark
	if (*pRead == '"') { pRead++; }
	
	*pStr = '\0';

	return czStrBuf;
}


bool MoCapFileReader::readTag(const char* czString)
{
	const char* czStr = readString();
	return _stricmp(czStr, czString) == 0;
}


