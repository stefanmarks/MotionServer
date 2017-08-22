/**
 * Motion Capture system that uses Cortex
 */

#pragma once

#include "Config.h"

#ifdef USE_CORTEX

#pragma comment(lib, "Cortex_SDK.lib")

#include "MoCapSystem.h"
#include "Configuration.h"
#include "Cortex.h"


class MoCapCortexConfiguration : public Configuration
{
public:
	MoCapCortexConfiguration();

	virtual bool handleArgument(unsigned int _idx, const std::string& _value);

public:

	bool        useCortex;
	std::string remoteCortexAddress;
	std::string localCortexAddress;
};



class MoCapCortex : public MoCapSystem
{
public:
	MoCapCortex(MoCapCortexConfiguration cofiguration);
	virtual ~MoCapCortex();

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
	 * Checks if the Cortex system handles unknown marker data.
	 *
	 * @return <code>true</code> if unknown markers are handled
	 */
	bool  isHandlingUnknownMarkers();

	/**
	 * Enables or disables handling of unknown marker data.
	 *
	 * @param enable  <code>true</code> to enable handling of unknown markers
	 */
	void  setHandleUnknownMarkers(bool enable);


private:

	/**
	 * Converts the scene description from Cortex to NatNet.
	 */
	void convertCortexDescriptionToNatNet(sBodyDefs& refCortex, sDataDescriptions& refDescr, sFrameOfMocapData& refFrame);

	/**
	 * Converts frame data from Cortex to NatNet.
	 */
	bool convertCortexFrameToNatNet(sFrameOfData& refCortex, sFrameOfMocapData& refFrame);
	void convertCortexMarkerToNatNet(tMarkerData& refCortex, MarkerData& refNatNet);
	void convertCortexMarkerSetToNatNet(sBodyData& refCortex, sMarkerSetData& refNatNet);
	void convertCortexSegmentToNatNet(double refCortex[], sRigidBodyData& refNatNet);
	void convertCortexSegmentsToNatNet(sBodyData& refCortex, sSkeletonData& refNatNet);

private:

	MoCapCortexConfiguration configuration;

	bool       initialised;
	bool       running;
	
	sHostInfo* pCortexInfo;

	float      unitScaleFactor;
	float      updateRate;
	bool       handleUnknownMarkers;

};

#endif // #ifdef USE_CORTEX