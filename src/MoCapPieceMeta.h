/**
 * Motion Capture system that uses PieceMeta
 */

#pragma once

#include "Config.h"

#ifdef USE_PIECEMETA

#include "MoCapSystem.h"
#include "Configuration.h"

#include <string>
#include <map>
#include <vector>

#include "json11.hpp"


class MoCapPieceMetaConfiguration : public Configuration
{
public:
	MoCapPieceMetaConfiguration();

	virtual bool handleArgument(unsigned int _idx, const std::string& _value);

public:

	bool                     usePieceMeta;
	int                      maximumFrameCount;
	bool                     listOnly;
	std::string              packageFilter;
	std::vector<std::string> channelFilters;
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

	// Forward declarations
	struct sPackage;
	struct sChannel;
	struct sStream;
	class  StreamConfiguration;


	/**
	 * Reads a URL into a string.
	 *
	 * @param url     the URL to read
	 * @param content the string to read the URL into
	 *
	 * @return <code>true</code> when read was successful, <code>false</code> if not.
	 */
	bool readURL(const std::string& url, std::string& content);


	/**
	 * Reads all packages from PieceMeta.
	 *
	 * @return list of package information.
	 */
	std::vector<sPackage> readPackages();


	/**
	 * Reads all channels from a package.
	 *
	 * @param package  the package structure to read and to fill in with the channel information
	 * 
	 * @return <code>true</code> if data was read successfully,
	 *         <code>false</code> if not
	 */
	bool readChannels(sPackage& package);


	/**
	 * Reads all streams from a channel.
	 *
	 * @param channel  the channel structure to read and to fill in with the stream information
	 *
	 * @return <code>true</code> if data was read successfully,
	 *         <code>false</code> if not
	 */
	bool readStreams(sChannel& channel);


	/**
	* Reads the data for a stream.
	*
	* @param stream  the stream to read the data for
	*
	* @return <code>true</code> if data was read successfully,
	*         <code>false</code> if not
	*/
	bool readStreamData(sStream& stream);


	/**
	 * Searches for the stream parameter configuration index by comparing the parameter names.
	 *
	 * @param names  the parameter name list to search for
	 *
	 * @return pointer to the stream configuration or NULL if no configuration matches
	 */
	const StreamConfiguration* findConfiguration(const std::vector<std::string>& names);


private:

	/**
	* Enumeration for the type of stream data, e.g., X position, timestamp
	*/
	enum eStreamType
	{
		timestamp, posX, posY, posZ, _last
	};
	
	/**
	 * Association of a stream name to a parameter type
	 */
	typedef std::map<const std::string, MoCapPieceMeta::eStreamType> tStreamConfiguration;
	
	/**
	 * Association of a stream type to the actual stream
	 */
	typedef std::map<eStreamType, sStream*> tStreamTypeMap;

	/**
	 * Association of a group name type to a stream type map
	 */
	typedef std::map<std::string, tStreamTypeMap> tStreamGroupMap;

	/**
	 * Inner class for a package
	 */
	struct MoCapPieceMeta::sPackage
	{
		std::string  title;
		std::string  description;
		std::string  uuid;

		std::vector<sChannel> channels;

		sPackage();
		sPackage(const json11::Json& json);

		sChannel* findChannel(const std::string& channelUUID);
		void      filterChannels(const std::vector<std::string>& channelFilter);
	};


	/**
	 * Inner class for a package channel
	 */
	struct MoCapPieceMeta::sChannel
	{
		sPackage*    package;

		std::string  title;
		std::string  uuid;

		sChannel*    pParent;

		std::vector<sStream>        streams;

		int                         frameCount;
		float                       frameRate;

		std::vector<std::string>    groupNames;
		std::vector<std::string>    streamNames;
		const StreamConfiguration*  pConfiguration;
		tStreamGroupMap             streamGroupMap;

		sChannel(sPackage& package, const json11::Json& json);
		
		void  analyseStreamData();
		
		void  setConfiguration(const StreamConfiguration* pConfiguration);
		
		float getTimestamp(int frame);
		void  getPosition(int frame, const std::string& group, float vecPos[3], bool reset);
	};


	/**
	 * Inner class for a channel stream
	 */
	struct MoCapPieceMeta::sStream
	{
		sChannel*   channel;

		std::string title;
		std::string uuid;
		std::string group;
		long        frameCount;
		float       fps;

		std::vector<float> data;

		sStream(sChannel& channel, const json11::Json& json, int maxFrameCount);
	};


private:

	MoCapPieceMetaConfiguration configuration;

	bool  initialised;
	bool  running;
	float updateRate;
	int   maxFrame, currentFrame;

	sPackage         activePackage;
	std::vector<int> activeChannels;
	int              longestChannel;

	char readBuffer[65536];

	static const StreamConfiguration STREAM_CONFIGURATIONS[];
};

#endif // #ifdef USE_PIECEMETA