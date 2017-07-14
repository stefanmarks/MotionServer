#include "MoCapPieceMeta.h"

#ifdef USE_PIECEMETA

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapPieceMeta"

#include <windows.h>
#include <WinInet.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <locale>

#include "json11.hpp"

#pragma comment(lib,"Wininet.lib")

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
	MoCapPieceMeta::StreamConfiguration( "XYZt", { "timestamp",  "x",  "y",  "z" }),
	MoCapPieceMeta::StreamConfiguration("vXYZt", { "timestamp", "vx", "vy", "vz" }),
	MoCapPieceMeta::StreamConfiguration("Unknown",{}) // this one must be last
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
	SystemConfiguration("PieceMeta"),
	usePieceMeta(false),
	packageFilter(""),
	channelFilter("")
{
	addParameter("-pieceMetaPackage", "<package name>",    "Load a PieceMeta package");
	addParameter("-channelFilter",    "<channel filter>",  "Filter to select channels with");
}


bool MoCapPieceMetaConfiguration::handleParameter(int idx, const std::string& value)
{
	bool success = true;
	switch (idx)
	{
		case 0: 
			packageFilter = value;
			usePieceMeta = true;
			break;

		case 1:
			channelFilter = value;

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
		LOG_INFO("Initialising");

		// query packages
		std::vector<sPackage> packages = readPackages();
		int packageCount = packages.size();
		if (packageCount > 0)
		{
			activePackage = packages[0]; // default: first package
			// rint packages and select active one by name
			LOG_INFO("Found " << packageCount << " packages:");
			for (int idx = 0; idx < packageCount; idx++)
			{
				const sPackage& package = packages[idx];
				LOG_INFO((idx + 1) << ": " << package.uuid << " - " << package.title);

				if ((package.uuid.find(configuration.packageFilter) != std::string::npos) ||
					(package.title.find(configuration.packageFilter) != std::string::npos))
				{
					activePackage = package;
				}
			}

			LOG_INFO("Active package: " << activePackage.title);

			readChannels(activePackage);
			int numChannelsUnfiltered = activePackage.channels.size();

			activePackage.filterChannels(configuration.channelFilter);
			int numChannels = activePackage.channels.size();
			LOG_INFO("Found " << numChannelsUnfiltered << " channels, " << numChannels << " filtered:");

			updateRate = 0;
			activeChannelIdx = -1;
			int totalStreams = 0;
			for (int cIdx = 0; cIdx < numChannels; cIdx++)
			{
				sChannel& channel = activePackage.channels[cIdx];
				readStreams(channel);
				channel.analyseStreamData();

				int numStreams = channel.streams.size();
				totalStreams += numStreams;
				
				LOG_INFO((cIdx + 1) << ": " << channel.uuid << " - " << channel.title << " - " <<
					numStreams << " streams, " << channel.frameCount << " frames, " << channel.frameRate << " FPS");

				LOG_INFO_START("  Groups: ");
				for (auto iter = channel.groupNames.cbegin(); iter != channel.groupNames.cend(); iter++)
				{
					LOG_INFO_MID(((iter == channel.groupNames.cbegin()) ? "" : ", ") << *iter);
				}
				LOG_INFO_END();

				LOG_INFO_START("  Streams: ");
				for (auto iter = channel.streamNames.cbegin(); iter != channel.streamNames.cend(); iter++)
				{
					LOG_INFO_MID(((iter == channel.streamNames.cbegin()) ? "" : ", ") << *iter);
				}

				channel.setConfiguration(findConfiguration(channel.streamNames));
				LOG_INFO_MID(" (Configuration " << channel.pConfiguration->getName() << ")");
				if (activeChannelIdx < 0 && channel.pConfiguration->isValid())
				{
					activeChannelIdx = cIdx;
					LOG_INFO_MID(" < Active");
				}
				LOG_INFO_END();

				LOG_INFO_START("  Dump: ");
				for (auto iter = channel.streams.cbegin(); iter != channel.streams.cend(); iter++)
				{
					LOG_INFO_MID(((iter == channel.streams.cbegin()) ? "" : ", ") << (*iter).group << "." << (*iter).title);
				}
				LOG_INFO_END();
			}

			if (activeChannelIdx >= 0)
			{
				// read frames
				LOG_INFO_START("Loading Stream Data:   0% ");
				int streamCount = 0;
				sChannel& channel = activePackage.channels[activeChannelIdx];
				int numStreams = channel.streams.size();
				for (int sIdx = 0; sIdx < numStreams; sIdx++)
				{
					streamCount++;
					LOG_INFO_MID("\b\b\b\b\b" << std::setw(3) << std::right << (streamCount * 100 / totalStreams) << "% ");
					sStream& stream = channel.streams[sIdx];
					readStreamData(stream);
				}
				LOG_INFO_END();

				updateRate   = channel.frameRate;
				currentFrame = 0;
				initialised  = true;

				/*
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
			else
			{
				LOG_WARNING("No suitable channel found");
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
		sChannel& channel = activePackage.channels[activeChannelIdx];
		
		int descrIdx = 0;
		
		// create markerset description and frame
		sMarkerSetDescription* pMarkerDesc = new sMarkerSetDescription();
		sMarkerSetData&        msData = refData.frame.MocapData[0];

		// name of marker set
		strcpy_s(pMarkerDesc->szName, sizeof(pMarkerDesc->szName), channel.title.substr(0, 4).c_str());
		strcpy_s(msData.szName, sizeof(msData.szName), pMarkerDesc->szName);

		// number of markers
		pMarkerDesc->nMarkers = channel.groupNames.size();
		msData.nMarkers       = pMarkerDesc->nMarkers;

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

		refData.description.nDataDescriptions = descrIdx;

		// pre-fill in frame data
		refData.frame.nMarkerSets = 1;

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
		sChannel& channel = activePackage.channels[activeChannelIdx];
		
		// get the frame number and timestamp
		refData.frame.iFrame     = currentFrame;
		refData.frame.fTimestamp = channel.getTimestamp(currentFrame);

		// do markers
		sMarkerSetData& msData = refData.frame.MocapData[0];
		for (int m = 0; m < msData.nMarkers; m++)
		{
			const std::string groupName = channel.groupNames.at(m);
			channel.getPosition(currentFrame, groupName, msData.Markers[m]);
		}

		currentFrame = (currentFrame + 1) % channel.frameCount;

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

	std::string request = PIECEMETA_BASE_URL "/packages/" + package.uuid + "/channels.json";
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
		LOG_INFO_MID(rotSymbol[rotSymbolIdx] << "\b");
		rotSymbolIdx = (rotSymbolIdx + 1) % sizeof(rotSymbol);

		int idxTo = min(idxFrom + stepsize, stream.frameCount);
		std::stringstream request;
		request << PIECEMETA_BASE_URL "/streams/" << stream.uuid << ".json"
		        << "?from=" << idxFrom << "&to=" << idxTo;
		std::string response;
		if (MoCapPieceMeta::readURL(request.str(), response))
		{
			std::string errorMsg;
			json11::Json json = json11::Json::parse(response, errorMsg);
			if (errorMsg.empty() && json.is_object())
			{
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


bool MoCapPieceMeta::readStreams(sChannel& channel)
{
	bool success = false;
	channel.streams.clear();

	std::string request = PIECEMETA_BASE_URL "/channels/" + channel.uuid + "/streams.json";
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
				sStream stream(channel, *i);
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


void MoCapPieceMeta::sPackage::filterChannels(const std::string& channelFilter)
{
	if (channelFilter.empty()) return;

	for (int i = channels.size() - 1 ; i >= 0 ; i--)
	{
		if (channels[i].title.find(channelFilter) == std::string::npos)
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

		// if not existing, add stream title to list of titles
		if (std::find(streamNames.begin(), streamNames.end(), stream.title) == streamNames.end())
		{
			streamNames.push_back(stream.title);
		}
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


void MoCapPieceMeta::sChannel::getPosition(int frame, const std::string& group, float vecPos[3])
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
					vecPos[tIdx] = stream->data[frame] / 100.0; // cm > m
				}
			}
		}
	}
}


/******************************************************************************
 * MoCapPieceMeta::sStream class
 */

MoCapPieceMeta::sStream::sStream(sChannel& channel, const json11::Json& json) :
	channel(&channel)
{
	if (json.is_object())
	{
		title      = json["title"].string_value();
		group      = json["group"].string_value();
		uuid       = json["uuid"].string_value();
		fps        = (float) json["fps"].number_value();
		frameCount = json["frameCount"].int_value();
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
