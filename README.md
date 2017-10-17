## Overview

_MotionServer_ is a Windows command line tool 
that converts motion capture data from multiple sources to the NatNet format
and streams it to the network.

Latest development sources can be found [on github](https://github.com/stefanmarks/MotionServer).

For copyright reasons, this project does not contain the sources, libraries, and DLLs for the Cortex and the NatNet SDKs.
Those files need to be downloaded from [OptiTrack](http://www.optitrack.com/products/natnet-sdk/),
or requested via email from [MotionAnalysis](http://www.motionanalysis.com/html/industrial/cortex.html).


## Folder structure

* `include/`  Folder for include files from the [NatNet SDK](http://www.optitrack.com/products/natnet-sdk/) 
              and other Motion Capture system SDKs (e.g., [Cortex](http://www.motionanalysis.com/html/industrial/cortex.html))
* `lib32/`    Folder for 32 bit libraries from the [NatNet SDK](http://www.optitrack.com/products/natnet-sdk/) 
              and other Motion Capture system SDKs (e.g., [Cortex](http://www.motionanalysis.com/html/industrial/cortex.html))
* `src/`      _MotionServer_ source files
* `Hardware`  Files related to hardware, e.g., the XBee interaction controller configuration files


## Command-Line Options

### Generic Operation
* `-h`                                   Print help to sceen and exit
* `-serverName <name>`                   Define the name of the MotionServer instance (default: `MotionServer`)
* `-serverAddress <address>`             Define the IP address of the MotionServer instance (default: `127.0.0.1`)
* `-multicastAddress <address>`          Define the Multicast IP Address of the MotionServer instance (default: disabled, using Unicast)
* `-interactionControllerPort <number>`  COM port of XBee interaction controller (default: 0=disabled, -1: scan for controller)
* `-readFile <filename>`                 Read MoCap data from a file
* `-writeFile`                           Write MoCap data into timestamped files

### Specific to Cortex
* `-cortexRemoteAddress <address>`  IP Address of the computer operating Cortex (can be `localhost` or `127.0.0.1`)
* `-cortexLocalAddress <address>`   IP Address of the local interface connecting to Cortex (usually only necessary in case of several network cards)

<!-- ### Examples
* `MotionServer.exe -serverAddress 127.0.0.1`
-->

## Commands during runtime

### Generic commands
* `q`  Quit server
* `r`  Restart server
* `p`  Pause/unpause server
* `d`  Print current scene description
* `f`  Print current scene data

### MoCap Module specific commands

#### Simulator
* `enableTrackingLoss`     Enables the loss of tracking for short periods of time
* `disableTrackingLoss`    Disables the loss of tracking (i.e., provides 100% reliable data)

#### Cortex
* `enableUnknownMarkers`   Send data for markers that cannot be associated with an actor (This data is not available in the Java and Unity client implementations - yet)
* `disableUnknownMarkers`  Do not send data for markers that cannot be associated with an actor


