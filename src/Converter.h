/**
 * Function headers for converting Cortex data structures to NatNet and managing memory.
 */

#include "Cortex.h"
#include "NatNetTypes.h"

float getUnitScaleFactor();
void  setUnitScaleFactor(float factor);

bool isHandlingUnknownMarkers();
void setHandleUnknownMarkers(bool enable);

void convertCortexDescriptionToNatNet(sBodyDefs& refCortex, sDataDescriptions& refDescr, sFrameOfMocapData& refFrame);
bool convertCortexFrameToNatNet(sFrameOfData& refCortex, sFrameOfMocapData& refFrame);

void freeNatNetDescription(sDataDescriptions& refDescr);
void freeNatNetFrameData(sFrameOfMocapData& refFrame);
