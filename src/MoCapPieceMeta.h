/**
 * Motion Capture system that uses PieceMeta
 */

#pragma once

#include "Config.h"

#ifdef USE_PIECEMETA

#include "MoCapSystem.h"
#include "SystemConfiguration.h"

#include <string>
#include <vector>

#include "json11.hpp"


class MoCapPieceMetaConfiguration : public SystemConfiguration
{
public:
	MoCapPieceMetaConfiguration();

	virtual bool handleParameter(int idx, const std::string& value);

public:

	bool        usePieceMeta;
	std::string packageFilter;
	std::string channelFilter;
};




class MoCapPieceMeta : public MoCapSystem
{
public:
	MoCapPieceMeta(MoCapPieceMetaConfiguration configuration);
	virtual ~MoCapPieceMeta();

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

private:

	struct sChannel;
	struct sStream;


	struct sPackage
	{
		std::string  title;
		std::string  description;
		std::string  uuid;

		std::vector<sChannel> channels;

		sPackage();
		sPackage(const json11::Json& json);

		sChannel* findChannel(const std::string& channelUUID);
		void      filterChannels(const std::string& channelFilter);
	};


	struct sChannel
	{
		sPackage*    package;

		std::string  title;
		std::string  uuid;

		sChannel*    pParent;

		std::vector<sStream> streams;

		sChannel(sPackage& package, const json11::Json& json);
	};


	struct sStream
	{
		sChannel*   channel;

		std::string title;
		std::string uuid;
		std::string group;
		long        frameCount;
		float       fps;

		std::vector<float> data;

		sStream(sChannel& channel, const json11::Json& json);
	};


	/**
	* Reads a URL into a string.
	*
	* @param url     the URL to read
	* @param content the string to read the URL into
	*
	* @return <code>true</code> when read was successful, <code>false</code> if not.
	*/
	bool readURL(const std::string& url, std::string& content);


	std::vector<sPackage> readPackages();

	bool readChannels(sPackage& package);

	bool readStreams(sChannel& channel);

	bool readStreamData(sStream& stream);


private:

	MoCapPieceMetaConfiguration configuration;

	bool  initialised;
	bool  isPlaying;
	float updateRate;

	sPackage activePackage;

	char readBuffer[65536];
};

#endif // #ifdef USE_PIECEMETA