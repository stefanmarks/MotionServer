## Overview

_MotionServer_ is a Windows command line tool 
that converts motion capture data from multiple sources to the NatNet format
and streams it to the network.

Latest development sources can be found [on github](https://github.com/stefanmarks/MotionServer).

For copyright reasons, this project does not contain the sources, libraries, and DLLs for the Cortex and the NatNet SDKs.
Those files need to be downloaded from [OptiTrack](http://www.optitrack.com/products/natnet-sdk/),
or requested via email from [MotionAnalysis](http://www.motionanalysis.com/html/industrial/cortex.html).


## Folder structure

* `include/`	Folder for include files from the [NatNet SDK](http://www.optitrack.com/products/natnet-sdk/) 
				and other Motion Capture system SDKs (e.g., [Cortex](http://www.motionanalysis.com/html/industrial/cortex.html))
* `lib32/`		Folder for 32 bit libraries from the [NatNet SDK](http://www.optitrack.com/products/natnet-sdk/) 
				and other Motion Capture system SDKs (e.g., [Cortex](http://www.motionanalysis.com/html/industrial/cortex.html))
* `src/`		_MotionServer_ source files

