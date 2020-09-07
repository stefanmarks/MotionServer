#include "MoCapData.h"

#include <stdlib.h>
#include <string.h>
#include <memory.h>


/******************************************************************************
 * MoCapData class
 */

MoCapData::MoCapData()
{
	reset();
}


MoCapData::~MoCapData()
{
	freeNatNetDescription();
	freeNatNetFrameData();
}


void MoCapData::reset()
{
	// reset data structure
	memset(&description, 0, sizeof(description));
	memset(&frame, 0, sizeof(frame));
}


void MoCapData::applyScale(float scale)
{
	for (int msIdx = 0; msIdx < frame.nMarkerSets; msIdx++)
	{
		sMarkerSetData& markerset = frame.MocapData[msIdx];
		for (int mIdx = 0; mIdx < markerset.nMarkers; mIdx++)
		{
			MarkerData& marker = markerset.Markers[mIdx];
			marker[0] *= scale;
			marker[1] *= scale;
			marker[2] *= scale;
		}
	}

	for (int rbIdx = 0; rbIdx < frame.nRigidBodies; rbIdx++)
	{
		sRigidBodyData& rigidBody = frame.RigidBodies[rbIdx];
		rigidBody.x *= scale;
		rigidBody.y *= scale;
		rigidBody.z *= scale;
		rigidBody.MeanError *= scale; // "abused" for bone length
	}

	for (int sIdx = 0; sIdx < frame.nSkeletons; sIdx++)
	{
		sSkeletonData& skeleton = frame.Skeletons[sIdx];
		for (int bIdx = 0; bIdx < frame.Skeletons[sIdx].nRigidBodies; bIdx++)
		{
			sRigidBodyData& bone = skeleton.RigidBodyData[bIdx];
			bone.x *= scale;
			bone.y *= scale;
			bone.z *= scale;
			bone.MeanError *= scale; // "abused" for bone length
		}
	}
}


sMarkerSetDescription* MoCapData::findMarkerSetDescription(const sMarkerSetData& refMarkerSetData) const
{
	sMarkerSetDescription* pResult = nullptr;
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		const sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		if (descr.type == Descriptor_MarkerSet)
		{
			// compare markersets by name
			if (strcmp(descr.Data.MarkerSetDescription->szName, refMarkerSetData.szName) == 0)
			{
				pResult = descr.Data.MarkerSetDescription;
				break;
			}
		}
	}
	return pResult;
}


sRigidBodyDescription* MoCapData::findRigidBodyDescription(const sRigidBodyData& refRigidBodyData) const
{
	sRigidBodyDescription* pResult = nullptr;
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		const sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		if (descr.type == Descriptor_RigidBody)
		{
			// compare rigid body by ID
			if (descr.Data.RigidBodyDescription->ID == refRigidBodyData.ID)
			{
				pResult = descr.Data.RigidBodyDescription;
				break;
			}
		}
	}
	return pResult;
}


sSkeletonDescription* MoCapData::findSkeletonDescription(const sSkeletonData& refSkeletonData) const
{
	sSkeletonDescription* pResult = nullptr;
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		const sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		if (descr.type == Descriptor_Skeleton)
		{
			// compare skeleton by ID
			if (descr.Data.SkeletonDescription->skeletonID == refSkeletonData.skeletonID)
			{
				pResult = descr.Data.SkeletonDescription;
				break;
			}
		}
	}
	return pResult;
}


sForcePlateDescription* MoCapData::findForcePlateDescription(const sForcePlateData& refForcePlateData) const
{
	sForcePlateDescription* pResult = nullptr;
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		const sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		if (descr.type == Descriptor_ForcePlate)
		{
			// compare force plate by ID
			if (descr.Data.ForcePlateDescription->ID == refForcePlateData.ID)
			{
				pResult = descr.Data.ForcePlateDescription;
				break;
			}
		}
	}
	return pResult;
}


sDeviceDescription* MoCapData::findDeviceDescription(const sDeviceData& refDeviceData) const
{
	sDeviceDescription* pResult = nullptr;
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		const sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		if (descr.type == Descriptor_Device)
		{
			// compare device by ID
			if (descr.Data.DeviceDescription->ID == refDeviceData.ID)
			{
				pResult = descr.Data.DeviceDescription;
				break;
			}
		}
	}
	return pResult;
}


void MoCapData::resetMarkerData(sMarkerSetData& refMarkerSetData) const
{
	for (int mIdx = 0; mIdx < refMarkerSetData.nMarkers; mIdx++)
	{
		MarkerData& refMarker = refMarkerSetData.Markers[mIdx];
		refMarker[0] = 0.0f;
		refMarker[1] = 0.0f;
		refMarker[2] = 0.0f;
	}
}


void MoCapData::resetRigidBodyData(sRigidBodyData&  refRigidBodyData) const
{
	refRigidBodyData.x = 0;
	refRigidBodyData.y = 0;
	refRigidBodyData.z = 0;
	refRigidBodyData.qw = 1;
	refRigidBodyData.qx = 0;
	refRigidBodyData.qy = 0;
	refRigidBodyData.qz = 0;
	refRigidBodyData.MeanError = 0;
	refRigidBodyData.params = STATUS_NOT_TRACKED;
}


void MoCapData::resetSkeletonData(sSkeletonData&  refSkeletonData) const
{
	for (int rIdx = 0; rIdx < refSkeletonData.nRigidBodies; rIdx++)
	{
		resetRigidBodyData(refSkeletonData.RigidBodyData[rIdx]);
	}
}


void MoCapData::freeNatNetDescription()
{
	for (int dataBlockIdx = 0; dataBlockIdx < description.nDataDescriptions; dataBlockIdx++)
	{
		sDataDescription& descr = description.arrDataDescriptions[dataBlockIdx];
		switch (descr.type)
		{
			case Descriptor_MarkerSet:
				// Marker set -> release array of marker names
				freeNatNetMarkerSetDescription(descr.Data.MarkerSetDescription);
				descr.Data.MarkerSetDescription = nullptr;
				break;

			case Descriptor_RigidBody:
				// Rigid body -> release data
				freeNatNetRigidBodyDescription(descr.Data.RigidBodyDescription);
				descr.Data.RigidBodyDescription = nullptr;
				break;

			case Descriptor_Skeleton:
				// Skeleton -> release data
				freeNatNetSkeletonDescription(descr.Data.SkeletonDescription);
				descr.Data.SkeletonDescription = nullptr;
				break;

			case Descriptor_ForcePlate:
				// Force plate -> release data
				freeNatNetForcePlateDescription(descr.Data.ForcePlateDescription);
				descr.Data.ForcePlateDescription = nullptr;
				break;

			case Descriptor_Device:
				// Periphery device -> release data
				freeNatNetDeviceDescription(descr.Data.DeviceDescription);
				descr.Data.DeviceDescription = nullptr;
				break;

			default:
				// nothing to release for the other descriptors
				break;
		}
		descr.type = 0;
	}
	description.nDataDescriptions = 0;
}


void MoCapData::freeNatNetMarkerSetDescription(sMarkerSetDescription* pMarkerSet)
{
	// release marker names
	for (int mIdx = 0; mIdx < pMarkerSet->nMarkers; mIdx++)
	{
		delete[] pMarkerSet->szMarkerNames[mIdx];
		pMarkerSet->szMarkerNames[mIdx] = nullptr;
	}
	
	// release marker names array
	delete[] pMarkerSet->szMarkerNames;
	pMarkerSet->szMarkerNames = nullptr;
	
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


void MoCapData::freeNatNetDeviceDescription(sDeviceDescription* pDevice)
{
	// release structure
	delete pDevice;
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

	// delete device data
	for (int iDeviceIdx = 0; iDeviceIdx < frame.nForcePlates; iDeviceIdx++)
	{
		freeNatNetDeviceData(frame.Devices[iDeviceIdx]);
	}
	frame.nDevices = 0;

	// delete unknown marker data
	delete[] frame.OtherMarkers;
	frame.nOtherMarkers = 0;
}


void MoCapData::freeNatNetMarkerSetData(sMarkerSetData& refMarkerSetData)
{
	delete[] refMarkerSetData.Markers;
	refMarkerSetData.Markers   = nullptr;
	refMarkerSetData.nMarkers  = 0;
}


void MoCapData::freeNatNetRigidBodySetData(sRigidBodyData& refBodySetData)
{
	// nothing to free
}


void MoCapData::freeNatNetSkeletonData(sSkeletonData& refSkeleton)
{
	for (int bIdx = 0; bIdx < refSkeleton.nRigidBodies; bIdx++)
	{
		freeNatNetRigidBodySetData(refSkeleton.RigidBodyData[bIdx]);
	}
	delete[] refSkeleton.RigidBodyData;
	refSkeleton.RigidBodyData = nullptr;
	refSkeleton.nRigidBodies  = 0;
}


void MoCapData::freeNatNetForcePlateData(sForcePlateData& refForcePlate)
{
	refForcePlate.nChannels = 0;
}


void MoCapData::freeNatNetDeviceData(sDeviceData& refDevice)
{
	refDevice.nChannels = 0;
}

