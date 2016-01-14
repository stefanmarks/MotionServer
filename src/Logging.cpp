#include "Logging.h"

#include <ios>
#include <iomanip>


void printMemory(std::ostream& refOutput, const void* pBuf, size_t length)
{
	// store previous stream settings
	std::stringstream strm;
	strm.fill('0');

	strm << std::hex << pBuf << ":" << std::endl; // address

	for (size_t offset = 0; offset < length; offset += 16)
	{
		strm << std::setw(4) << offset << ": ";

		for (size_t b = 0; b < 16; b++ )
		{
			if ( offset + b < length )
			{ 
				strm << std::setw(2) << (int)((uint8_t*)pBuf)[offset + b] << " ";
			}
			else
			{
				strm << "   ";
			}
		}

		strm << "  ";
		for (size_t b = 0; b < 16; b++)
		{
			if (offset + b < length)
			{
				uint8_t c = ((uint8_t*)pBuf)[offset + b];

				if ((c >= ' ') && (c <= 127))
				{
					strm << (char)c;
				}
				else
				{
					strm << '.';
				}
			}
		}
		strm << std::endl;		
	}

	refOutput << strm.str();
}


void printModelDefinitions(std::ostream& refOutput, sDataDescriptions& refData)
{
	refOutput << "Model Description (" << refData.nDataDescriptions << " blocks)" << std::endl;
	for (int dIdx = 0; dIdx < refData.nDataDescriptions; dIdx++)
	{
		refOutput << "Block " << dIdx << ":\t";
		switch (refData.arrDataDescriptions[dIdx].type)
		{
			case Descriptor_MarkerSet:
			{
				sMarkerSetDescription* pMS = refData.arrDataDescriptions[dIdx].Data.MarkerSetDescription;
				refOutput << "Markerset '" << pMS->szName
					<< "' with " << pMS->nMarkers << " Markers" << std::endl;
				for (int mIdx = 0; mIdx < pMS->nMarkers; mIdx++)
				{
					refOutput << "\t" << mIdx << ":\t" << pMS->szMarkerNames[mIdx] << std::endl;
				}
				break;
			}
			
			case Descriptor_RigidBody:
			{
				sRigidBodyDescription* pRB = refData.arrDataDescriptions[dIdx].Data.RigidBodyDescription;
				refOutput << "RigidBody '" << pRB->szName << "' "
					<< "(ID: " << pRB->ID
					<< ", Parent: " << pRB->parentID
					<< ", Offset: [" << pRB->offsetx << ", " << pRB->offsety << ", " << pRB->offsetz << "]"
					<< ")" << std::endl;
				break;
			}

			case Descriptor_Skeleton:
			{
				sSkeletonDescription* pSK = refData.arrDataDescriptions[dIdx].Data.SkeletonDescription;
				refOutput << "Skeleton '" << pSK->szName << "' "
					<< "(ID: " << pSK->skeletonID
					<< ", #Bones: " << pSK->nRigidBodies
					<< ")" << std::endl;
				for (int bIdx = 0; bIdx < pSK->nRigidBodies; bIdx++)
				{
					sRigidBodyDescription* pRB = &pSK->RigidBodies[bIdx];
					refOutput << "\t" << bIdx << ":\t"
						<< "Bone '" << pRB->szName << "' "
						<< "(ID: " << pRB->ID
						<< ", Parent: " << pRB->parentID
						<< ", Offset: [" << pRB->offsetx << ", " << pRB->offsety << ", " << pRB->offsetz << "]"
						<< ")" << std::endl;
				}
				break;
			}

			case Descriptor_ForcePlate:
			{
				sForcePlateDescription* pFP = refData.arrDataDescriptions[dIdx].Data.ForcePlateDescription;
				refOutput << "Force Plate '" << pFP->strSerialNo << "' "
					<< "(ID: " << pFP->ID
					<< ", #Channels: " << pFP->nChannels
					<< ")" << std::endl;
				for (int cIdx = 0; cIdx < pFP->nChannels; cIdx++)
				{
					refOutput << "\tChannel " << cIdx << ": '" << pFP->szChannelNames[cIdx] << "'" << std::endl;
				}
				break;
			}

			default:
			{
				refOutput << "Unknown data descriptor " << refData.arrDataDescriptions[dIdx].type << std::endl;
			}
		}
	}
}


void printFrameOfData(std::ostream& refOutput, sFrameOfMocapData& refData)
{
	refOutput << "Frame Data ("
		<< "Frame# " << refData.iFrame 
		<< ", Latency: " << (((int)(refData.fLatency * 1000)) / 1000.0f) << "s" 
		<< ")" << std::endl;

	// print markersets and marker positions
	for (int msIdx = 0; msIdx < refData.nMarkerSets; msIdx++)
	{
		sMarkerSetData& refMarkerset = refData.MocapData[msIdx];
		refOutput << "Markerset #" << msIdx << " ('" << refMarkerset.szName << "'):" << std::endl;
		for (int mIdx = 0; mIdx < refMarkerset.nMarkers; mIdx++)
		{
			MarkerData& refMarker = refMarkerset.Markers[mIdx];
			refOutput << "\tMarker #" << mIdx << ":" <<
				"\tX=" << refMarker[0] << ", Y=" << refMarker[1] << ", Z=" << refMarker[2] << std::endl;
		}
	}

	// print rigid bodies
	for (int rbIdx = 0; rbIdx < refData.nRigidBodies; rbIdx++)
	{
		sRigidBodyData& refRigidBody = refData.RigidBodies[rbIdx];
		refOutput << "RigidBody #" << rbIdx 
			<< " (ID " << refRigidBody.ID 
			<< ", " << (((refRigidBody.params & 0x01) != 0) ? "Tracked" : "Not Tracked")
			<< "):" << std::endl;
		for (int mIdx = 0; mIdx < refRigidBody.nMarkers; mIdx++)
		{
			MarkerData& refMarker = refRigidBody.Markers[mIdx];
			refOutput << "\tRB Marker #" << refRigidBody.MarkerIDs[mIdx] << ":" <<
				"\tX=" << refMarker[0] << ", Y=" << refMarker[1] << ", Z=" << refMarker[2] << std::endl;
		}
		refOutput << "\tPosition:    "
			"X=" << refRigidBody.x << ", Y=" << refRigidBody.y << ", Z=" << refRigidBody.z << std::endl;
		refOutput << "\tOrientation: "
			"X=" << refRigidBody.qx << ", Y=" << refRigidBody.qy << ", Z=" << refRigidBody.qz << ", W=" << refRigidBody.qw << std::endl;
	}

	// print skeletons
	for (int skIdx = 0; skIdx < refData.nSkeletons; skIdx++)
	{
		sSkeletonData& refSkeleton = refData.Skeletons[skIdx];
		refOutput << "Skeleton #" << skIdx << " (ID " << refSkeleton.skeletonID << "):" << std::endl;
		for (int rbIdx = 0; rbIdx < refSkeleton.nRigidBodies; rbIdx++)
		{
			sRigidBodyData& refRigidBody = refSkeleton.RigidBodyData[rbIdx];
			refOutput << "\tRB #" << refRigidBody.ID 
				<< " (" << (((refRigidBody.params & 0x01) != 0) ? "Tracked" : "Not Tracked")
				<< ", Length: " << refRigidBody.MeanError
				<< "):" << std::endl
				<< "\t\tPosition:    X=" << refRigidBody.x << ", Y=" << refRigidBody.y << ", Z=" << refRigidBody.z << std::endl 
				<< "\t\tOrientation: X=" << refRigidBody.qx << ", Y=" << refRigidBody.qy << ", Z=" << refRigidBody.qz << ", W=" << refRigidBody.qw << std::endl;
		}
	}

	// print force plate data (as interaction device data)
	for (int fpIdx = 0; fpIdx < refData.nForcePlates; fpIdx++)
	{
		sForcePlateData& refForcePlate = refData.ForcePlates[fpIdx];
		refOutput << "Device #" << fpIdx << " (ID " << refForcePlate.ID << "):" << std::endl;
		for (int chIdx = 0; chIdx < refForcePlate.nChannels; chIdx++)
		{
			sAnalogChannelData& refChannel = refForcePlate.ChannelData[chIdx];
			refOutput << "\tChn #" << chIdx << ":\t" << refChannel.Values[0] << std::endl;
		}
	}
}


