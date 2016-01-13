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
				freeNatNetMarkerSetDescription(description.arrDataDescriptions[dataBlockIdx].Data.MarkerSetDescription);
				break;

			case Descriptor_RigidBody:
				// Rigid body -> release data
				freeNatNetRigidBodyDescription(description.arrDataDescriptions[dataBlockIdx].Data.RigidBodyDescription);
				break;

			case Descriptor_Skeleton:
				// Skeleton -> release data
				freeNatNetSkeletonDescription(description.arrDataDescriptions[dataBlockIdx].Data.SkeletonDescription);
				break;

			case Descriptor_ForcePlate:
				// Force plate -> release data
				freeNatNetForcePlateDescription(description.arrDataDescriptions[dataBlockIdx].Data.ForcePlateDescription);
				break;

			default:
				// nothing to release for the other descriptors
				break;
		}
	}
	description.nDataDescriptions = 0;
}


void MoCapData::freeNatNetMarkerSetDescription(sMarkerSetDescription* pMarkerSet)
{
	// release marker names
	for (int mIdx = 0; mIdx < pMarkerSet->nMarkers; mIdx++)
	{
		delete[] pMarkerSet->szMarkerNames[mIdx];
	}
	
	// release marker names array
	delete[] pMarkerSet->szMarkerNames;
	pMarkerSet->szMarkerNames = NULL;
	
	// release structure
	delete pMarkerSet;
}


void MoCapData::freeNatNetRigidBodyDescription(sRigidBodyDescription* pRigidBody)
{
	// release structure
	delete pRigidBody;
}


void MoCapData::freeNatNetSkeletonDescription(sSkeletonDescription* pSkeleton)
{
	// release structure
	delete pSkeleton;
}


void MoCapData::freeNatNetForcePlateDescription(sForcePlateDescription* pForcePlate)
{
	// release structure
	delete pForcePlate;
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

	// delete force plate data
	for (int iForcePlateIdx = 0; iForcePlateIdx < frame.nForcePlates; iForcePlateIdx++)
	{
		freeNatNetForcePlateData(frame.ForcePlates[iForcePlateIdx]);
	}
	frame.nForcePlates = 0;

	// delete unknown marker data
	delete[] frame.OtherMarkers;
	frame.nOtherMarkers = 0;
}


void MoCapData::freeNatNetMarkerSetData(sMarkerSetData& refMarkerSetData)
{
	delete[] refMarkerSetData.Markers;
	refMarkerSetData.Markers = NULL;
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
	for (int bIdx = 0; bIdx < refSkeleton.nRigidBodies; bIdx++)
	{
		freeNatNetRigidBodySetData(refSkeleton.RigidBodyData[bIdx]);
	}
	delete[] refSkeleton.RigidBodyData;
	refSkeleton.RigidBodyData = NULL;
	refSkeleton.nRigidBodies  = 0;
}


void MoCapData::freeNatNetForcePlateData(sForcePlateData& refForcePlate)
{
	refForcePlate.nChannels = 0;
}


