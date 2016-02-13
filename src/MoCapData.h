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
	void freeNatNetMarkerSetDescription(sMarkerSetDescription* pMarkerSet);
	void freeNatNetRigidBodyDescription(sRigidBodyDescription* pRigidBody);
	void freeNatNetSkeletonDescription(sSkeletonDescription* pSkeleton);
	void freeNatNetForcePlateDescription(sForcePlateDescription* pForcePlate);

	void freeNatNetFrameData();
	void freeNatNetMarkerSetData(sMarkerSetData& refMarkerSetData);
	void freeNatNetRigidBodySetData(sRigidBodyData& refBodySetData);
	void freeNatNetSkeletonData(sSkeletonData& refSkeleton);
	void freeNatNetForcePlateData(sForcePlateData& refForcePlate);

public:
	sDataDescriptions description;
	sFrameOfMocapData frame;

};

