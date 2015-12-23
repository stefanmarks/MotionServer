#include "MoCapData.h"

#include <stdlib.h>
#include <memory.h>


MoCapData::MoCapData()
{
	// reset data structures
	memset(&description, 0, sizeof(description));
	memset(&frame, 0, sizeof(frame));
}


MoCapData::~MoCapData()
{
	freeNatNetDescription();
	freeNatNetFrameData();
}


void MoCapData::freeNatNetDescription()
{
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		switch (description.arrDataDescriptions[dataBlockIdx].type)
		{
			case Descriptor_MarkerSet:
				// Marker set -> release array of marker names
				freeNatNetMarkerSetDescription(*description.arrDataDescriptions[dataBlockIdx].Data.MarkerSetDescription);
				break;

			case Descriptor_RigidBody:
				// Rigid body -> release data
				freeNatNetRigidBodyDescription(*description.arrDataDescriptions[dataBlockIdx].Data.RigidBodyDescription);
				break;

			case Descriptor_Skeleton:
				// Skeleton -> release data
				freeNatNetSkeletonDescription(*description.arrDataDescriptions[dataBlockIdx].Data.SkeletonDescription);
				break;

			default:
				// nothing to release for the other descriptors
				break;
		}
	}
	description.nDataDescriptions = 0;
}


void MoCapData::freeNatNetMarkerSetDescription(sMarkerSetDescription& refMarkerSet)
{
	for (int mIdx = 0; mIdx < refMarkerSet.nMarkers; mIdx++)
	{
		delete[] refMarkerSet.szMarkerNames[mIdx];
		refMarkerSet.szMarkerNames[mIdx] = NULL;
	}
	// release array
	delete[] refMarkerSet.szMarkerNames;
	refMarkerSet.szMarkerNames = NULL;
	refMarkerSet.szName[0] = '\0';
	refMarkerSet.nMarkers = 0;
}


void MoCapData::freeNatNetRigidBodyDescription(sRigidBodyDescription& refRigidBody)
{
	// nothing to release
}


void MoCapData::freeNatNetSkeletonDescription(sSkeletonDescription& refSkeleton)
{
	// nothing to release
}


void MoCapData::freeNatNetFrameData()
{
	// delete marker sets
	for (int iMarkerSetIdx = 0; iMarkerSetIdx < frame.nMarkerSets; iMarkerSetIdx++)
	{
		freeNatNetMarkerSetData(frame.MocapData[iMarkerSetIdx]);
	}
	frame.nMarkerSets = 0;

	// delete physical bodies
	for (int iBodySetIdx = 0; iBodySetIdx < frame.nRigidBodies; iBodySetIdx++)
	{
		freeNatNetRigidBodySetData(frame.RigidBodies[iBodySetIdx]);
	}
	frame.nRigidBodies = 0;

	// delete skeleton data
	for (int iSkeletonIdx = 0; iSkeletonIdx < frame.nSkeletons; iSkeletonIdx++)
	{
		freeNatNetSkeletonData(frame.Skeletons[iSkeletonIdx]);
	}
	frame.nSkeletons = 0;

	// delete unknown marker data
	delete[] frame.OtherMarkers;
	frame.nOtherMarkers = 0;
}


void MoCapData::freeNatNetMarkerSetData(sMarkerSetData& refMarkerSetData)
{
	delete[] refMarkerSetData.Markers;
	refMarkerSetData.Markers = NULL;
	refMarkerSetData.szName[0] = '\0';
	refMarkerSetData.nMarkers = 0;
}


void MoCapData::freeNatNetRigidBodySetData(sRigidBodyData& refBodySetData)
{
	delete[] refBodySetData.Markers;
	refBodySetData.Markers = NULL;
	delete[] refBodySetData.MarkerIDs;
	refBodySetData.MarkerIDs = NULL;
	delete[] refBodySetData.MarkerSizes;
	refBodySetData.MarkerSizes = NULL;
	refBodySetData.nMarkers = 0;
}


void MoCapData::freeNatNetSkeletonData(sSkeletonData& refSkeleton)
{
	delete[] refSkeleton.RigidBodyData;
	refSkeleton.RigidBodyData = NULL;
	refSkeleton.nRigidBodies = 0;
	refSkeleton.skeletonID = 0;
}

