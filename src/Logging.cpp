#include "Logging.h"


void printModelDefinitions(std::ostream& refOutput, sDataDescriptions& refData)
{
	refOutput << "Model Description (" << refData.nDataDescriptions << " blocks)" << std::endl;
	for (int dIdx = 0; dIdx < refData.nDataDescriptions; dIdx++)
	{
		refOutput << dIdx << ":\t";
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
				refOutput << "Force plate descriptor" << std::endl;
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
	refOutput << "Frame Data (#" << refData.iFrame << ")" << std::endl;
	for (int mIdx = 0; mIdx < refData.nMarkerSets; mIdx++)
	{
	}
}


