/**
 * Class for keeping MoCap description and frame data together.
 * Also performs proper copying and cleanup of data structures.
 */

#pragma once

#include "NatNetTypes.h"

class MoCapData
{
public:
	MoCapData();
	~MoCapData();

private:
	// Internal methods for freeing dynamically allocated data structures
	void freeNatNetDescription();
	void freeNatNetMarkerSetDescription(sMarkerSetDescription& refMarkerSet);
	void freeNatNetRigidBodyDescription(sRigidBodyDescription& refRigidBody);
	void freeNatNetSkeletonDescription(sSkeletonDescription& refSkeleton);

	void freeNatNetFrameData();
	void freeNatNetMarkerSetData(sMarkerSetData& refMarkerSetData);
	void freeNatNetRigidBodySetData(sRigidBodyData& refBodySetData);
	void freeNatNetSkeletonData(sSkeletonData& refSkeleton);

public:
	sDataDescriptions description;
	sFrameOfMocapData frame;

};

