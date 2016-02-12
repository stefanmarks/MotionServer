/**
 * Interface for motion capture data providers.
 */

#pragma once

#include "MoCapData.h"
#include <string>


// this function can be used by the class to stream frames
extern void signalNewFrame();

/**
 * Pure virtual base class for the minimum MoCap system methods.
 */

class MoCapSystem
{
public:

	/**
	 * Initialises the MoCap system.
	 *
	 * @return <code>true</code> if the initialisation was successful
	 */
	virtual bool initialise() = 0;

	/**
	 * Checks if the MoCap system is active.
	 *
	 * @return <code>true</code> if the MoCap system is active
	 */
	virtual bool isActive() = 0;

	/**
	 * Gets the ideal update rate per second for the system.
	 *
	 * @return the minimum update rate for the MoCap system in times per second
	 */
	virtual float getUpdateRate() = 0;

	/**
	 * Forces an update of the MoCap data.
	 *
	 * @return <code>true</code> if the update was successful
	 */
	virtual bool update() = 0;

	/**
	 * Gets a scene description for the NatNet data structure.
	 *
	 * @param refData  reference to the data structure to fill in
	 *
	 * @return <code>true</code> when the method completed successfully
	 */
	virtual bool getSceneDescription(MoCapData& refData) = 0;

	/**
	 * Gets the data for a single frame as a NatNet data structure.
	 *
	 * @param refData  reference to the data structure to fill in
	 *
	 * @return <code>true</code> when the method completed successfully
	 */
	virtual bool getFrameData(MoCapData& refData) = 0;

	/**
	 * Processes a custom string command.
	 *
	 * @param strCommand  the command to execute
	 *
	 * @return <code>true</code> when the command was execded successfully
	 */
	virtual bool processCommand(const std::string& strCommand) = 0;

	/**
	 * Deinitialises the MoCap system.
	 *
	 * @return <code>true</code> when the system was deinitialised successfully
	 */
	virtual bool deinitialise() = 0;


	virtual ~MoCapSystem() { };

};
