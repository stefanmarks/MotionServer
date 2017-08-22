#include "MoCapPieceMeta.h"

#ifdef USE_PIECEMETA

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapPieceMeta"

#include <windows.h>
#include <WinInet.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>

#include "json11.hpp"

#pragma comment(lib,"Wininet.lib")

// uncomment to create file with package information
// #define DUMP_PACKAGE_INFO_TO_FILE

#define PIECEMETA_BASE_URL "http://api.piecemeta.com/"


/**
 * Class for managing stream parameter name and type associations
 */
class MoCapPieceMeta::StreamConfiguration
{
public:
	StreamConfiguration(const std::string& name, const std::vector<std::string>& config);

	const std::string& getName() const;

	bool isValid() const;

	bool matches(const std::vector<std::string>& nameList) const;

	eStreamType getStreamType(const sStream& stream) const;

private:

	std::string          name;
	tStreamConfiguration configuration;
};


/**
 * List of stream parameter configurations
 */
const MoCapPieceMeta::StreamConfiguration MoCapPieceMeta::STREAM_CONFIGURATIONS[] =
{
	MoCapPieceMeta::StreamConfiguration("t/x/y/z",    { "timestamp",  "x",  "y",  "z" }),
	MoCapPieceMeta::StreamConfiguration("t/vx/vy/vz", { "timestamp", "vx", "vy", "vz" }),
	MoCapPieceMeta::StreamConfiguration("T/X/Y/Z",    { "Time",       "X",  "Y",  "Z" }),
	MoCapPieceMeta::StreamConfiguration("Tl/X/Y/Z",   { "Timestamp_LowPart", "X", "Y", "Z" }), // Timestamp_LowPart, TrackedSkeleton, #Frame, Validity, X, Y, Z, STATUS
	MoCapPieceMeta::StreamConfiguration("Unknown",    { }) // this one must be last
};

#define STREAM_CONFIGURATION_COUNT (sizeof(STREAM_CONFIGURATIONS) / sizeof(STREAM_CONFIGURATIONS[0]))



/******************************************************************************
 * StreamConfiguration class
 */

MoCapPieceMeta::StreamConfiguration::StreamConfiguration(const std::string& name, const std::vector<std::string>& config) :
	name(name)
{
	// store strings and associted parameter types in the map
	int  lastIdx = min(config.size(), _last - timestamp);
	for (int idx = 0; idx < lastIdx; idx++)
	{
		configuration[config[idx]] = eStreamType(timestamp + idx);
	}
}


const std::string& MoCapPieceMeta::StreamConfiguration::getName() const
{
	return name;
}


bool MoCapPieceMeta::StreamConfiguration::isValid() const
{
	return !configuration.empty();
}


bool MoCapPieceMeta::StreamConfiguration::matches(const std::vector<std::string>& nameList) const
{
	bool match = true;
	for (auto configIter = configuration.cbegin(); configIter != configuration.cend(); configIter++)
	{
		// if only a single parameter name doesn't exist, this is not a match
		if (std::find(nameList.begin(), nameList.end(), (*configIter).first) == nameList.end())
		{
			match = false;
			break;
		}
	}
	return match;
}


MoCapPieceMeta::eStreamType MoCapPieceMeta::StreamConfiguration::getStreamType(const sStream& stream) const
{
	auto typeIter = configuration.find(stream.title);
	return (typeIter == configuration.cend()) ? _last : typeIter->second;
}


/******************************************************************************
 * MoCapPieceMetaConfiguration class
 */

MoCapPieceMetaConfiguration::MoCapPieceMetaConfiguration() :
	Configuration("PieceMeta"),
	usePieceMeta(false),
	maximumFrameCount(INT_MAX),
	listOnly(false),
	packageFilter(""),
	channelFilters()
{
	addParameter("-pieceMetaPackage", "<package name>",    "Load a PieceMeta package");
	addParameter("-channelFilter",    "<channel filter>",  "Filter to select channels with (this option can be used multiple times)");
	addParameter("-maxFrame",         "<frame number>",    "Read data only up to the given frame number");
	addOption(   "-listOnly",                              "Only list the packages and filtered channels, but do not start the actual server");
}


bool MoCapPieceMetaConfiguration::handleArgument(unsigned int _idx, const std::string& _value)
{
	bool success = true;
	switch (_idx)
	{
		case 0: 
			packageFilter = _value;
			usePieceMeta = true;
			break;

		case 1:
			channelFilters.push_back(_value);
			break;

		case 2:
			maximumFrameCount = atoi(_value.c_str());
			break;

		case 3:
			listOnly = true;
			break;

		default: 
			success = false;
			break;
	}
	return success;
}



/******************************************************************************
 * MoCapPieceMeta class
 */

MoCapPieceMeta::MoCapPieceMeta(MoCapPieceMetaConfiguration configuration) :
	configuration(configuration),
	initialised(false),
	running(true),
	updateRate(100.0f)
{
	
}


bool MoCapPieceMeta::initialise()
{
	if (!initialised && configuration.usePieceMeta)
	{
		if (!configuration.listOnly) LOG_INFO("Initialising");

		// query packages
		std::vector<sPackage> packages = readPackages();
		int packageCount = packages.size();
		if (packageCount > 0)
		{
			if (configuration.listOnly)
			{
				LOG_INFO("Found " << packageCount << " packages:");
			}
			
			// select active package by name
			activePackage = packages[0]; // default: first package
			for (int idx = 0; idx < packageCount; idx++)
			{
				const sPackage& package = packages[idx];
				if ((package.uuid.find(configuration.packageFilter)  != std::string::npos) ||
					(package.title.find(configuration.packageFilter) != std::string::npos))
				{
					activePackage = package;
				}

				if (configuration.listOnly)
				{
					LOG_INFO((idx + 1) << ": " << package.uuid << " - " << package.title);
				}
			}
			LOG_INFO("Active package: " << activePackage.title);

			// read channel data
			readChannels(activePackage);
			int numChannelsUnfiltered = activePackage.channels.size();

			// filter the channels
			activePackage.filterChannels(configuration.channelFilters);
			int numChannels = activePackage.channels.size();
			
			if (configuration.listOnly)
			{
				LOG_INFO("Found " << numChannelsUnfiltered << " channels, " << numChannels << " filtered:");
			}

			// analyse the channels
			updateRate     = 0;
			maxFrame       = 0;
			longestChannel = 0;
			activeChannels.clear();
			
			for (int cIdx = 0; cIdx < numChannels; cIdx++)
			{
				sChannel& channel = activePackage.channels[cIdx];
				readStreams(channel);
				channel.analyseStreamData();
				
				if (configuration.listOnly)
				{
					LOG_INFO(cIdx << ": " << channel.uuid << " - " << channel.title << " - " <<
						channel.streams.size() << " streams, " << 
						channel.frameCount << " frames, " << 
						channel.frameRate << " FPS");

					std::stringstream groups;
					for (auto iter = channel.groupNames.cbegin(); iter != channel.groupNames.cend(); iter++)
					{
						if (iter != channel.groupNames.cbegin())
						{
							groups << ", ";
						}
						groups << *iter;
					}
					LOG_INFO("   Groups: " << groups.str());

					std::stringstream streams; 
					for (auto iter = channel.streamNames.cbegin(); iter != channel.streamNames.cend(); iter++)
					{
						if (iter != channel.streamNames.cbegin())
						{
							streams << ", ";
						}
						streams << *iter;
					}
					LOG_INFO("   Streams: " << streams.str());

#ifdef DUMP_PACKAGE_INFO_TO_FILE
				std::stringstream s;
				s << activePackage.title << "\t" << activePackage.channels.size() << "\t" <<
					channel.uuid << "\t" << channel.title << "\t" << channel.frameCount << "\t" << channel.frameRate << "\t" <<
					groups.str() << "\t" << streams.str() << std::endl;
				std::ofstream logfile("dump.tsv", std::ofstream::app);
				logfile << s.str();
				logfile.close();
#endif
				}

				// what sort of stream configuration do we have here?
				channel.setConfiguration(findConfiguration(channel.streamNames));
				if (configuration.listOnly)
				{
					LOG_INFO("   Configuration: " << channel.pConfiguration->getName());
				}

				if (channel.pConfiguration->isValid())
				{
					// this channel is active
					activeChannels.push_back(cIdx);
					// which channel has most frames?
					if (channel.frameCount > activePackage.channels[longestChannel].frameCount)
					{
						longestChannel = cIdx;
					}
				}

				/*
				LOG_INFO_START("  Dump: ");
				for (auto iter = channel.streams.cbegin(); iter != channel.streams.cend(); iter++)
				{
					LOG_INFO_MID(((iter == channel.streams.cbegin()) ? "" : ", ") << (*iter).group << "." << (*iter).title);
				}
				LOG_INFO_END();
				*/
			}

			// only read actual stream data when not merely printing the package/channel list
			if (!configuration.listOnly)
			{
				if (activeChannels.size() > 0)
				{
					// load data for all active channels
					for (size_t cIdx = 0; cIdx < activeChannels.size(); cIdx++)
					{
						int channelIdx = activeChannels[cIdx];
						sChannel& channel = activePackage.channels[channelIdx];

						LOG_INFO_START("Channel " << channelIdx << 
							" (#" << cIdx <<
							", " << channel.title <<
							", " << channel.frameCount << " frames"
							", " << channel.frameRate << " fps"
							", " << channel.pConfiguration->getName() <<
							"): Loading stream data...   0% ");

						// read frames
						int numStreams  = channel.streams.size();
						for (int sIdx = 0; sIdx < numStreams; sIdx++)
						{
							LOG_INFO_MID("\b\b\b\b\b" << std::setw(3) << std::right << (sIdx * 100 / numStreams) << "% ");
							sStream& stream = channel.streams[sIdx];
							readStreamData(stream);
						}
						LOG_INFO_MID("\b\b\b\b\b100%  ");
						LOG_INFO_END();

						if (updateRate == 0)
						{
							// first channel defines framerate
							updateRate = channel.frameRate;
						}
						else if (updateRate != channel.frameRate)
						{
							// framerate should be the same for all channels
							LOG_WARNING("Different framerate for channel " << cIdx);
						}

						maxFrame = max(maxFrame, channel.frameCount);

						/*
						// print data of the 10 first frames
						for (int frame = 0; frame < 10; frame++)
						{
							LOG_INFO("  Frame " << frame << ":");
							for (auto iter = channel.groupNames.cbegin(); iter != channel.groupNames.cend(); iter++)
							{
								float vec[3] = { 0,0,0 };
								channel.getPosition(100, *iter, vec);
								LOG_INFO("    " << *iter << ": " << vec[0] << "," << vec[1] << "," << vec[2]);
							}
						}
						*/
					}

					currentFrame = 0;
					initialised  = true;
				}
				else
				{
					LOG_WARNING("No suitable channel found");
				}
			}
		}
	}
	return initialised;
}


bool MoCapPieceMeta::isActive()
{
	return initialised;
}


float MoCapPieceMeta::getUpdateRate()
{
	return updateRate;
}


bool MoCapPieceMeta::isRunning()
{
	return running;
}


void MoCapPieceMeta::setRunning(bool running)
{
	this->running = running;
}


bool MoCapPieceMeta::update()
{
	if (initialised && running)
	{
		signalNewFrame();
	}
	return true;
}


bool MoCapPieceMeta::getSceneDescription(MoCapData& refData)
{
	bool success = false;
	if (initialised)
	{
		int descrIdx       = 0;
		int markersetCount = 0;

		// run through all active channels
		for (size_t cIdx = 0 ; cIdx < activeChannels.size() ; cIdx++)
		{
			int channelIdx = activeChannels[cIdx];
			sChannel& channel = activePackage.channels[channelIdx];

			// create markerset description and frame
			sMarkerSetDescription* pMarkerDesc = new sMarkerSetDescription();
			sMarkerSetData&        msData = refData.frame.MocapData[markersetCount];

			// name of marker set = channel index
			sprintf_s(pMarkerDesc->szName, "Channel%d", cIdx);
			strcpy_s(msData.szName, sizeof(msData.szName), pMarkerDesc->szName);

			// number of markers
			pMarkerDesc->nMarkers = channel.groupNames.size();
			msData.nMarkers = pMarkerDesc->nMarkers;

			pMarkerDesc->szMarkerNames = new char*[pMarkerDesc->nMarkers];
			msData.Markers = new MarkerData[msData.nMarkers];

			for (int m = 0; m < pMarkerDesc->nMarkers; m++)
			{
				pMarkerDesc->szMarkerNames[m] = _strdup(channel.groupNames.at(m).c_str());
			}

			// add to description list
			refData.description.arrDataDescriptions[descrIdx].type = Descriptor_MarkerSet;
			refData.description.arrDataDescriptions[descrIdx].Data.MarkerSetDescription = pMarkerDesc;
			descrIdx++;
			markersetCount++;
		}

		refData.description.nDataDescriptions = descrIdx;

		// pre-fill in frame data
		refData.frame.nMarkerSets = markersetCount;

		success = true;
		LOG_INFO("Requesting scene description")
	}

	return success;
}


bool MoCapPieceMeta::getFrameData(MoCapData& refData)
{
	bool success = false;

	if (initialised)
	{
		// get the frame number and timestamp of the longest channel
		refData.frame.iFrame     = currentFrame;
		refData.frame.fTimestamp = activePackage.channels[longestChannel].getTimestamp(currentFrame);

		for (size_t cIdx = 0; cIdx < activeChannels.size(); cIdx++)
		{
			int channelIdx = activeChannels[cIdx];
			sChannel& channel = activePackage.channels[channelIdx];

			// do markers
			sMarkerSetData& msData = refData.frame.MocapData[cIdx];
			for (int mIdx = 0; mIdx < msData.nMarkers; mIdx++)
			{
				const std::string groupName = channel.groupNames.at(mIdx);
				channel.getPosition(currentFrame, groupName, msData.Markers[mIdx], true);
			}
		}

		currentFrame = (currentFrame + 1) % maxFrame;

		success = true;
	}

	return success;
}


bool MoCapPieceMeta::processCommand(const std::string& strCommand)
{
	bool processed = false;

	return processed;
}


bool MoCapPieceMeta::deinitialise()
{
	if (initialised)
	{
		LOG_INFO("Deinitialised");

		initialised = false;
	}
	return !initialised;
}


MoCapPieceMeta::~MoCapPieceMeta()
{
	deinitialise();
}


bool MoCapPieceMeta::readURL(const std::string& url, std::string &content)
{
	bool success = false;
	
	// prepare request:

	HINTERNET hInt = InternetOpen(
		TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:54.0) Gecko/20100101 Firefox/54.0"), 
		INTERNET_OPEN_TYPE_PRECONFIG, 
		NULL, NULL, 0);
	DWORD dwRequestFlags = INTERNET_FLAG_NO_UI   // no UI please
		| INTERNET_FLAG_NO_AUTH           // don't authenticate
		| INTERNET_FLAG_PRAGMA_NOCACHE    // do not try the cache or proxy
		| INTERNET_FLAG_NO_CACHE_WRITE;   // don't add this to the IE cache

	// convert URL into WChar
	std::wstring wURL;
	wURL.assign(url.begin(), url.end());

	// open URL
	HINTERNET hUrl = InternetOpenUrl(hInt, wURL.c_str(), NULL, 0, dwRequestFlags, NULL);
	if (hUrl)
	{
		// success: read content
		DWORD dwBytesRead = 0;
		content.clear();
		while (InternetReadFile(hUrl, readBuffer, sizeof(readBuffer) - 1, &dwBytesRead) && (dwBytesRead > 0))
		{
			readBuffer[dwBytesRead] = '\0';
			content += readBuffer;
		}
		InternetCloseHandle(hUrl);
		success = true;
	}
	else
	{
		LOG_ERROR("Could not open " << url);
	}
	InternetCloseHandle(hInt);
	return success;
}


std::vector<MoCapPieceMeta::sPackage> MoCapPieceMeta::readPackages()
{
	std::vector<sPackage> packages;
	std::string response;
	if (readURL(PIECEMETA_BASE_URL "packages.json", response))
	{
		std::string errorMsg;
		json11::Json json = json11::Json::parse(response, errorMsg);
		if (errorMsg.empty() && json.is_array())
		{
			const std::vector<json11::Json>& packageJson = json.array_items();
			for (auto i = packageJson.cbegin(); i != packageJson.cend(); ++i)
			{
				sPackage package(*i);
				packages.push_back(package);
			}
		}
		else
		{
			LOG_ERROR("Could not read packages (" << errorMsg << ")");
		}

	}
	return packages;
}


bool MoCapPieceMeta::readChannels(sPackage& package)
{
	bool success = false;
	package.channels.clear();

	std::string request = PIECEMETA_BASE_URL "packages/" + package.uuid + "/channels.json";
	std::string response;
	if (MoCapPieceMeta::readURL(request, response))
	{
		std::string errorMsg;
		json11::Json json = json11::Json::parse(response, errorMsg);
		if (errorMsg.empty() && json.is_array())
		{
			const std::vector<json11::Json>& channelJson = json.array_items();
			for (std::vector<json11::Json>::const_iterator i = channelJson.cbegin(); i != channelJson.cend(); ++i)
			{
				sChannel channel(package, *i);
				package.channels.push_back(channel);
			}
			success = true;
		}
		else
		{
			LOG_ERROR("Could not read channels (" << errorMsg << ")");
		}
	}
	return success;
}


bool MoCapPieceMeta::readStreams(sChannel& channel)
{
	bool success = false;
	channel.streams.clear();

	std::string request = PIECEMETA_BASE_URL "channels/" + channel.uuid + "/streams.json";
	std::string response;
	if (MoCapPieceMeta::readURL(request, response))
	{
		std::string errorMsg;
		json11::Json json = json11::Json::parse(response, errorMsg);
		if (errorMsg.empty() && json.is_array())
		{
			const std::vector<json11::Json>& channelJson = json.array_items();
			for (std::vector<json11::Json>::const_iterator i = channelJson.cbegin(); i != channelJson.cend(); ++i)
			{
				sStream stream(channel, *i, configuration.maximumFrameCount);
				channel.streams.push_back(stream);
			}
			success = true;
		}
		else
		{
			LOG_ERROR("Could not read stream (" << errorMsg << ")");
		}
	}
	return success;
}


bool MoCapPieceMeta::readStreamData(sStream& stream)
{
	const char rotSymbol[]  = { '|', '/', '-', '\\'};
	int        rotSymbolIdx = 0;

	bool success = true;
	stream.data.clear();
	
	int idxFrom  = 0;
	int stepsize = 6000;
	while ((idxFrom < stream.frameCount) && success)
	{
		// display rotating bar
		LOG_INFO_MID(rotSymbol[rotSymbolIdx] << "\b");
		rotSymbolIdx = (rotSymbolIdx + 1) % sizeof(rotSymbol);

		// determine from/to indices and build query
		int idxTo = min(idxFrom + stepsize, stream.frameCount);
		std::stringstream request;
		request << PIECEMETA_BASE_URL "streams/" << stream.uuid << ".json"
		        << "?from=" << idxFrom << "&to=" << idxTo;
		
		// execute query
		std::string response;
		if (MoCapPieceMeta::readURL(request.str(), response))
		{
			std::string errorMsg;
			json11::Json json = json11::Json::parse(response, errorMsg);
			if (errorMsg.empty() && json.is_object())
			{
				// data received > parse
				const std::vector<json11::Json>& channelJson = json["frames"].array_items();
				for (std::vector<json11::Json>::const_iterator i = channelJson.cbegin(); i != channelJson.cend(); ++i)
				{
					stream.data.push_back((float) (*i).number_value());
				}
				idxFrom = idxTo;
			}
			else
			{
				LOG_ERROR("Could not read stream data (" << errorMsg << ")");
				success = false;
			}
		}
	}
	return success;
}


/******************************************************************************
 * MoCapPieceMeta::sPackage class
 */

MoCapPieceMeta::sPackage::sPackage() :
	title(""),
	uuid("")
{}


MoCapPieceMeta::sPackage::sPackage(const json11::Json& json)
{
	if (json.is_object())
	{
		title       = json["title"].string_value();
		description = json["description"].string_value();
		uuid        = json["uuid"].string_value();
	}
}


/******************************************************************************
 * MoCapPieceMeta::sChannel class
 */

MoCapPieceMeta::sChannel* MoCapPieceMeta::sPackage::findChannel(const std::string& channelUUID)
{
	for (std::vector<sChannel>::iterator i = channels.begin(); i != channels.end(); i++)
	{
		if ((*i).uuid == channelUUID) return &(*i);
	}
	return NULL;
}


void MoCapPieceMeta::sPackage::filterChannels(const std::vector<std::string>& channelFilters)
{
	if (channelFilters.empty()) return;

	for (int i = channels.size() - 1 ; i >= 0 ; i--)
	{
		bool filterOut = true;
		for (auto channelFilterIter = channelFilters.cbegin(); channelFilterIter != channelFilters.cend(); channelFilterIter++)
		{
			const std::string& channelFilter = *channelFilterIter;
			if ((channels[i].title.find(channelFilter) != std::string::npos) ||
				(channels[i].uuid.find(channelFilter)  != std::string::npos))
			{
				// at least one match: don't remove
				filterOut = false;
			}
		}
		if (filterOut)
		{
			channels.erase(channels.begin() + i);
		}
	}
}


MoCapPieceMeta::sChannel::sChannel(sPackage& package, const json11::Json& json) :
	package(&package),
	pConfiguration(NULL)
{
	if (json.is_object())
	{
		title = json["title"].string_value();
		uuid  = json["uuid"].string_value();

		std::string parentUUID = json["parent_channel_uuid"].string_value();
		pParent = package.findChannel(parentUUID);
	}
}


void MoCapPieceMeta::sChannel::analyseStreamData()
{
	groupNames.clear();
	streamNames.clear();
	frameCount = 0;
	frameRate  = 0;

	int numStreams = streams.size();
	for (auto sIter = streams.cbegin() ; sIter != streams.end() ; sIter++)
	{
		const sStream& stream = *sIter;
		frameCount = max(stream.frameCount, frameCount);
		frameRate  = max(stream.fps, frameRate);

		// if not existing, add stream group to list of groups
		if (!stream.group.empty() && std::find(groupNames.begin(), groupNames.end(), stream.group) == groupNames.end())
		{
			groupNames.push_back(stream.group);
		}
		std::sort(groupNames.begin(), groupNames.end());

		// if not existing, add stream title to list of titles
		if (std::find(streamNames.begin(), streamNames.end(), stream.title) == streamNames.end())
		{
			streamNames.push_back(stream.title);
		}
		std::sort(streamNames.begin(), streamNames.end());
	}
}


void MoCapPieceMeta::sChannel::setConfiguration(const StreamConfiguration* pConfiguration)
{
	this->pConfiguration = pConfiguration;
	streamGroupMap.clear();
	// go through all streams
	for (auto sIter = streams.begin(); sIter != streams.end(); sIter++)
	{
		sStream& stream = *sIter;

		// find the stream group type map
		auto groupIter = streamGroupMap.find(stream.group);
		if (groupIter == streamGroupMap.end())
		{
			streamGroupMap[stream.group]; // make sure the entry exists
			groupIter = streamGroupMap.find(stream.group);
		}

		// insert stream into type map of that group
		eStreamType type = pConfiguration->getStreamType(stream);
		groupIter->second[type] = &stream;
	}
}


float MoCapPieceMeta::sChannel::getTimestamp(int frame)
{
	float timestamp = 0;
	
	// find the empty group (there is only one timestamp per channel)
	auto groupIter = streamGroupMap.find("");
	if (groupIter != streamGroupMap.end())
	{
		// found it > find the timestamp stream
		auto typeMap = (*groupIter).second;
		auto streamIter = typeMap.find(eStreamType::timestamp);
		if (streamIter != typeMap.end())
		{
			// found it > get timestamp value
			auto stream = (*streamIter).second;
			if ((frame >= 0) && (frame < stream->frameCount))
			{
				timestamp = stream->data[frame];
			}
		}
	}
	return timestamp;
}


void MoCapPieceMeta::sChannel::getPosition(int frame, const std::string& group, float vecPos[3], bool reset)
{
	// find the group
	auto groupIter = streamGroupMap.find(group);
	if (groupIter != streamGroupMap.end())
	{
		auto typeMap = (*groupIter).second;
		// iterate through X/Y/Z position types
		const eStreamType arrTypes[] = { posX, posY, posZ };
		for (int tIdx = 0; tIdx < 3; tIdx++)
		{
			auto streamIter = typeMap.find(arrTypes[tIdx]);
			if (streamIter != typeMap.end())
			{
				// found the type > get the value
				auto stream = (*streamIter).second;
				if ((frame >= 0) && (frame < stream->frameCount))
				{
					vecPos[tIdx] = stream->data[frame];
				}
				reset = false;
			}
		}
	}

	if (reset)
	{
		// no data found > reset to 0
		vecPos[0] = vecPos[1] = vecPos[2] = 0;
	}
}


/******************************************************************************
 * MoCapPieceMeta::sStream class
 */

MoCapPieceMeta::sStream::sStream(sChannel& channel, const json11::Json& json, int maxFrameCount) :
	channel(&channel)
{
	if (json.is_object())
	{
		title      = json["title"].string_value();
		group      = json["group"].string_value();
		uuid       = json["uuid"].string_value();
		fps        = (float) json["fps"].number_value();
		frameCount = json["frameCount"].int_value();
		frameCount = min(frameCount, maxFrameCount); // limit to maximum
	}
}


const MoCapPieceMeta::StreamConfiguration* MoCapPieceMeta::findConfiguration(const std::vector<std::string>& names)
{
	int configIdx = -1;
	for (int idx = 0; idx < STREAM_CONFIGURATION_COUNT; idx++)
	{
		if (STREAM_CONFIGURATIONS[idx].matches(names))
		{
			return &STREAM_CONFIGURATIONS[idx];
		}
	}
	return NULL;
}


#endif // #ifdef USE_PIECEMETA
