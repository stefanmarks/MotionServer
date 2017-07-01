#include "MoCapPieceMeta.h"

#ifdef USE_PIECEMETA

#include "Logging.h"
#undef   LOG_CLASS
#define  LOG_CLASS "MoCapPieceMeta"

#include <windows.h>
#include <WinInet.h>
#include <iostream>
#include <string>
#include <locale>

#include "json11.hpp"

#pragma comment(lib,"Wininet.lib")

#define PIECEMETA_BASE_URL "http://api.piecemeta.com/"


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




MoCapPieceMeta::MoCapPieceMeta(MoCapPieceMetaConfiguration configuration) :
	configuration(configuration),
	initialised(false),
	isPlaying(true),
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
			for (int idx = 0 ; idx < packageCount ; idx++)
			{ 
				const sPackage& package = packages[idx];
				LOG_INFO((idx + 1) << ": " << package.uuid << " - " << package.title);

				if ((package.uuid.find(configuration.packageFilter)  != std::string::npos) || 
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
			int totalStreams = 0;
			for (int cIdx = 0; cIdx < numChannels; cIdx++)
			{
				sChannel& channel = activePackage.channels[cIdx];
				readStreams(channel);

				// gather stream stats
				int   numStreams = channel.streams.size();
				int   maxFrameCount = 0;
				float maxFPS = 0;
				totalStreams += numStreams;
				for (int sIdx = 0; sIdx < numStreams; sIdx++)
				{
					sStream& stream = channel.streams[sIdx];
					maxFrameCount = max(stream.frameCount, maxFrameCount);
					maxFPS        = max(stream.fps, maxFPS);
				}
				LOG_INFO((cIdx + 1) << ": " << channel.uuid << " - " << channel.title << " - " <<
					numStreams << " streams, " << maxFrameCount << " frames, " << maxFPS << " FPS");

				// determine maximum FPS
				updateRate = max(updateRate, maxFPS);
			}

			// read frames
			LOG_INFO("Loading Stream Data:");
			for (int cIdx = 0; cIdx < numChannels; cIdx++)
			{
				LOG_INFO_START("- Channel " << (cIdx + 1));
				sChannel& channel = activePackage.channels[cIdx];
				int   numStreams = channel.streams.size();
				for (int sIdx = 0; sIdx < numStreams; sIdx++)
				{
					LOG_INFO_MID(".");
					sStream& stream = channel.streams[sIdx];
					readStreamData(stream);
				}
				LOG_INFO_END();
			}

			initialised = true;
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
	return isPlaying;
}


void MoCapPieceMeta::setRunning(bool running)
{
	isPlaying = running;
}


bool MoCapPieceMeta::update()
{
	return true;
}


bool MoCapPieceMeta::getSceneDescription(MoCapData& refData)
{
	bool success = false;
	if (initialised)
	{
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
			for (std::vector<json11::Json>::const_iterator i = packageJson.cbegin(); i != packageJson.cend(); ++i)
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
	package(&package)
{
	if (json.is_object())
	{
		title = json["title"].string_value();
		uuid  = json["uuid"].string_value();

		std::string parentUUID = json["parent_channel_uuid"].string_value();
		pParent = package.findChannel(parentUUID);
	}
}



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

#endif // #ifdef USE_PIECEMETA
