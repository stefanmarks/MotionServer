/**
 * Class for keeping MoCap description and frame data together.
 * Also performs proper copying and cleanup of data structures.
 */

#pragma once

#include "NatNetTypes.h"

// constants for the RigidBody.param field
#define STATUS_NOT_TRACKED ((short) 0x00)
#define STATUS_TRACKED     ((short) 0x01)


class MoCapData
{
public:
	MoCapData();
	~MoCapData();

	void reset();

	void applyScale(float scale);

public:
	sMarkerSetDescription*  findMarkerSetDescription( const sMarkerSetData&  refMarkerSetData) const;
	sRigidBodyDescription*  findRigidBodyDescription( const sRigidBodyData&  refRigidBodyData) const;
	sSkeletonDescription*   findSkeletonDescription(  const sSkeletonData&   refSkeletonData) const;
	sForcePlateDescription* findForcePlateDescription(const sForcePlateData& refForcePlateData) const;

	void resetMarkerData(   sMarkerSetData& refMarkerSetData) const;
	void resetSkeletonData( sSkeletonData&  refSkeletonData) const;

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

