## MythTV External Recorder Protocol (Version 3)
This document defines the communication protocol between MythTV (mythbackend) and the external recorder application. The communication uses JSON-formatted messages exchanged over a standard communication channel (e.g., standard I/O or TCP socket).
## General Structure

* All requests must contain at least a command string and a numeric serial identifier.
* All responses return the requested command, the tracking serial number, a status flag (OK, WARN, ERROR), and an optional informational message.

------------------------------
## 1. Initialization and Information

## APIVersion
Negotiates the protocol version. This application specifically implements version 3. This command is always "bare" to support previous protocol version.

* Query Example:

"APIVersion?"

* Response Example:

"OK:3"

## Version?
Retrieves the software version string of the external recorder application.

* Query Example:

{"command": "Version?", "serial": 2}

* Response Example:

{"command": "APIVersion", "message": "v4.5", "serial": "1", "status": "OK"}


## Description?
Retrieves a short identification or description of the current running application instance.

* Query Example:

{"command": "Description?", "serial": 3}

* Response Example:

{"command": "Description?", "message": "mag-1-2-3", "serial": "3", "status": "OK"}


------------------------------
## 2. Capabilities and Capabilities Negotiation

## HasTuner?
Queries whether the current external recorder instance supports tuning capabilities.

* Query Example:

{"command": "HasTuner?", "serial": 4}

* Response Example:

{"command": "HasTuner", "message": "Yes", "serial": "2", "status": "OK"}


## HasPictureAttributes?
Queries whether picture modifications (brightness, contrast, etc.) are supported. This program never supports picture attributes.

* Query Example:

{"command": "HasPictureAttributes?", "serial": 5}

* Response Example:

{"command": "HasPictureAttributes", "message": "No", "serial": "4", "status": "OK"}


## FlowControl?
Queries the streaming control mechanism used by the recorder. This program strictly expects software flow control.

* Query Example:

{"command": "FlowControl?", "serial": 6}

* Response Example:

{"command": "FlowControl?", "message": "XON/XOFF", "serial": "5", "status": "OK"}


## BlockSize
Configures or informs the backend of the expected block transfer size for data operations.

* Query Example:

{"command": "BlockSize", "serial": 7, "value": "3080192"}

* Response Example:

{"command": "BlockSize", "message": "Blocksize 3080192", "serial": "6", "status": "OK"}


------------------------------
## 3. Tuning and Status Control

## LockTimeout?
Retrieves the maximum allowed duration (in milliseconds) to wait for a hardware lock during tuning.

* Query Example:

{"command": "LockTimeout?", "serial": 8}

* Response Example:

{"command": "LockTimeout", "message": "30000", "serial": "8", "status": "OK"}


## TuneChannel
Requests the recorder to change channels. Contains extensive metadata detailing the physical and logical channel parameters.

* Query Example:

{"atsc_major":355,"atsc_minor":0,"callsign":"CNBCHD","chanid":20355,"channum":"355","command":"TuneChannel","description":"The front lines of market coverage; host Scott Wapner and the top investors get to the heart of the action as it's happening and help set the agenda for the rest of the day.","duration":3600,"freqid":"355","inputid":16,"mplexid":0,"name":"CNBC HD Stream","programid":"EP043359570571","recordid":25131,"serial":13,"seriesid":"EP04335957","sourceid":2,"subtitle":"","title":"Halftime Report","value":"355"}

{
  "command": "TuneChannel",
  "serial": 9,
  "chanid": 100,
  "channum": "100",
  "callsign": "CALLSIGN",
  "name": "Station Name",
  "title": "Title",
  "subtitle": "Subtitle",
  "description": "",
  "seriesid": "",
  "programid": "",
  "recordid": 4165,
  "duration": 1923,
  "inputid": 16,
  "sourceid": 4,
  "mplexid": 0,
  "freqid": "",
  "atsc_major": 0,
  "atsc_minor": 0,
  "value": "96"
}

* Response Example:

{"command": "TuneChannel", "message": "InProgress `/usr/local/bin/roku-control --channum 318`\"", "serial": "9", "status": "OK"}

## TuneStatus?

* Query Example:

{"command":"TuneStatus?","serial":10}

* Response Example:



## SignalStrengthPercent?
Queries the current internal tuning progress or hardware signal quality metric.

* Query Example:

{"command": "SignalStrengthPercent?", "serial": 11}

* Response Example:

{"command": "SignalStrengthPercent?", "serial": 11}


## HasLock?
Queries whether the recorder has successfully locked onto the stream payload.

* Query Example:

{"command": "HasLock?", "serial": 12}

* Response Example:

{"command": "HasLock?", "serial": 12}


## IsOpen?
Queries whether the external recording application pipeline has finished initialization and launched successfully.

* Query Example:

{"command": "IsOpen?", "serial": 13}

* Response Example:

{"command": "IsOpen?", "message": "Not Open yet", "serial": "13", "status": "WARN"}


------------------------------
## 4. Streaming and Flow Control

## StartStreaming
Instructs the engine to begin internal processing and multiplexing of transport stream data frames.

* Query Example:

{"command": "StartStreaming", "serial": 14}

* Response Example:

{"command": "StartStreaming", "message": "Streaming Started", "serial": "11", "status": "OK"}


## XON
Issued by mythbackend when it is ready to consume data. Opens the transport stream output buffer.

* Query Example:

{"command": "XON", "serial": 15}

* Response Example:

{"command": "XON", "message": "Started Streaming", "serial": "12", "status": "OK"}


## XOFF
Issued by mythbackend if the data transmission rate exceeds buffer limits or when pausing ingestion. Suspends stream delivery.

* Query Example:

{"command": "XOFF", "serial": 16}

* Response Example:

{"command": "XOFF", "message": "Stopped Streaming", "serial": "13", "status": "OK"}


## StopStreaming
Instructs the engine to halt pipeline processing and data delivery maps.

* Query Example:

{"command": "StopStreaming", "serial": 17}

* Response Example:

{"command": "StopStreaming", "message": "Streaming Stopped", "serial": "12", "status": "OK"}


------------------------------
## 5. Channel Ingestion and Discovery

## LoadChannels
Triggers internal routines to compile the list of available channels and returns the configuration count.

* Query Example:

{"command": "LoadChannels", "serial": 19}

* Response Example:

{"command": "LoadChannels", "message": "52", "serial": "19", "status": "OK"}


## FirstChannel
Retrieves information mapping the first channel entry generated during the configuration cycle.

* Query Example:

{"command": "FirstChannel", "serial": 20}

* Response Example:

{"command": "FirstChannel", "message": "ChanNum,ChanName,Callsign,xmltvid,icon", "serial": "20", "status": "OK"}


## NextChannel
Retrieves channel mapping attributes for the subsequent entry in the internal iterator array.

* Query Example:

{"command": "NextChannel", "serial": 21}

* Response Example:

{"command": "NextChannel", "message": "ChanNum,ChanName,Callsign,xmltvid,icon", "serial": "21", "status": "OK"}


------------------------------
## 6. Shutdown

## CloseRecorder
Signals the application to perform memory optimization routines, flush active buffers, and terminate safely.

* Query Example:

{"command": "CloseRecorder", "serial": 18}

* Response Example:

{"command": "CloseRecorder", "message": "Terminating", "serial": "9", "status": "OK"}
