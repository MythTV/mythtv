
# MythExternRecorder
---
## Overview

MythExternRecorder uses the "External (black box) recorder" protocol (see `Protocol.md`). Users create a TOML-style configuration file to specify how the external recorder application is controlled. The recorder application could be `ffmpeg`, `cvlc`, or any other application that produces a transport stream containing audio and video.

When using `http://localhost:6544` to configure a new capture card, choose "External (black box) recorder" as the card type, then specify the path to `mythexternrecorder` along with the argument for your configuration file:

```text
/usr/bin/mythexternrecorder --conf /home/mythtv/etc/hdhrrecorder-1.toml
```

---
## Sections

There are seven sections that can be included in the configuration file:

* **VARIABLES** (optional) - User-defined `key=value` pairs used to simplify the configuration.
* **RECORDER** (required) - Contains the system command used to generate the transport stream. It can also specify a `desc`, which is used to identify this instance in the log files.
* **TUNER** (optional) - Contains the system commands used to tune and manage the channel. It can also specify a `channels` file containing channel-specific information.
* **SCANNER** (optional) - Contains a system command to run before loading channels into MythTV. Typically used to create the channels conifguration file.
* **TOUCH** (optional) - Used to spin up one or more background tasks. For example, some internet-based streams will timeout after 4 hours unless actively "touched".
* **INCLUDES** (optional) - Lists other TOML configuration files to be included.

If `[VARIABLES]` is provided, it must reside in the main TOML file, but all other sections can be offloaded to an included file.

---
### VARIABLES

A `[VARIABLES]` section may be specified to consolidate common information. For example:

```toml
[VARIABLES]
INPUT=1
DEVICE="roku1"
CODEC="hevc_qsv"
```

Any variable defined in this section can be referenced in any other section using the `{var_name}` syntax.

In addition to user-defined variables, several built-in variables are automatically provided by MythTV when tuning:

* **ATSC_MAJOR** – ATSC major channel number (e.g., `5` for channel 5.1)
* **ATSC_MINOR** – ATSC minor channel number (e.g., `1` for channel 5.1)
* **CALLSIGN** – Channel callsign
* **CHANID** – Internal MythTV channel ID
* **CHANNUM** – Channel number
* **DESCRIPTION** – Episode description
* **DURATION** – Episode duration
* **FREQID** – Channel frequency ID
* **INPUTID** – Internal MythTV capture card input ID
* **INPUTNAME** - MythTV input display name
* **MPLEXID** – Channel multiplex ID
* **NAME** – Channel name
* **PROGRAMID** – Unique program ID (e.g., `EP043359570571`)
* **RECORDID** – Internal MythTV recording rule ID
* **SERIESID** – Episode series ID
* **SOURCEID** – Internal MythTV video source ID
* **TITLE** – Episode title
* **SUBTITLE** – Episode subtitle

> **NOTE:**
> These variables are only populated during the tuning process. If your configuration does not define `[TUNER]/command` or `[TUNER]/channels`, they will be unavailable.

If a `[TUNER]/channels` file is used (see below), a `URL` variable may also be defined individually for each channel.

#### Optional Variable Syntax
Especially when using an include file, it can be desirable to define a command that changes behavior depending on existance of a variable. That is achieved by wrapping it in "[? ... ?]" blocks:

```
[RECORDER]
command = "[?taskset -c {CORE1}?] /usr/local/bin/magewell2ts --realtime -b {BOARD} -i {INPUT} -m -c {CODEC} -q 21 {LOG}"
```

If the variable CORE1 is not defined, then the whole "taskset -c " block would be removed before executing the command.

All of the variables inside "[? ... ?]" must be defined for the block to be preserved. For an example, you might have anywhere from one to four core variables defined, and that could be handled by using multiple "[? ... ?] blocks:

```
command = "[?taskset -c {CORE1}?][?,{CORE2}?][?,{CORE3}?][?,{CORE4}?] /usr/local/bin/magewell2ts --realtime -b {BOARD} -i {INPUT} -m -c {CODEC} -q 21 {LOG}"
```

---
### RECORDER

* **command** (required) – Specifies the system command to execute. This command must produce a transport stream.
* **desc** (optional) – Used in the `mythbackend` log files to identify this recorder instance. In addition to the `mythbackend` log, `mythexternrecorder` creates its own independent log file, named using this description if provided. If desc is not provided, then the log file will be the same name as the config file but with a log extension.
* **shell** (optional) - If defined, all commands will be run via this shell. **NOTE:** except for [INCLUDE]/files and [RECORDER]/desc, when a shell is specified, variables are **not** expanded by mythexternrecorder and the varaibles are instead injected into the shells environment. Be sure to use your shell's syntax for dealing with any variables.

```toml
[RECORDER]
command="/usr/local/bin/magewell2ts [?-b {BOARD}?] -i {INPUT} -m -c {CODEC} {LOG}"
desc="{DEVICE}-{BOARD}-{INPUT}-{SOURCE}"
```

You can also leverage MythTV recording profiles to specify customized commands for different quality levels:

```toml
[RECORDER]
command_lowquality="command with lower quality parameters"
command_highquality="command with higher quality parameters"
command_default="command with default quality"
command="fallback command"
```

If a recording is initiated using the "High Quality" profile and `[RECORDER]/command_highquality` is specified, it will be executed. Otherwise, `[RECORDER]/command` will be used as the fallback.

---
### TUNER

The following parameters can be placed in the `[TUNER]` section and will be executed at their respective times:

* **command** (optional) – The system command used to tune to channel. If mythexternrecorer is already tuned to the requested channel then it will not re-run this command. See **newepisode**.
* **channels** (optional) – Path to a TOML configuration file specifying channel-specific tuning parameters. If a channel is matched, its parameters will override the associated command in the main`[TUNER]` section.
* **newepisode** (optional) – Executed when MythTV indicates that a new episode is starting on the same channel that is already tuned. In this scenario, `mythexternrecorder` skips `[TUNER]/command` and executes `[TUNER]/newepisode` instead.
* **recstarting** (optional) – Executed when MythTV completes tuning tasks and is about to start consuming the transport stream for recording.
* **recstarted** (optional) – Executed after MythTV has successfully started consuming the transport stream.
* **recfinished** (optional) – Executed when MythTV is actively shutting down the external recorder instance.
* **timeout** (optional) – Specifies how long (in milliseconds) to wait for a successful tune before marking the recording as failing.

```toml
[TUNER]
channels="/home/mythtv/etc/yttv-channels.toml"
command="stb-control --device {DEVICE} --link {URL}"
newepisode="stb-control --device {DEVICE} --touch"
recstarting="stb-control --device {DEVICE} --prologue"
recstarted="stb-control --device {DEVICE} --mark"
recfinished="stb-control --device {DEVICE} --reset"
timeout=5000
```

---
#### Channel Overrides

If `[TUNER]/channels` is provided, it must point to a TOML configuration file containing per-channel tuning information. If any of the "tuning event" commands are specified within the channels file for a given channel, those commands will override the corresponding commands specified in the main `[TUNER]` section.

In addition, the `channels.toml` configuration file can specify a `URL` variable for each channel. If provided, this value will be injected into the available variables list and can be referenced across all tuning commands, including those in the main `[TUNER]` section.

Channels are searched first by their `CALLSIGN`. If no match is found, an attempt is made to match by channel number (`CHANNUM`). For example:

**input-1.toml**
```toml
[TUNER]
command="stb-control --device {DEVICE} {URL}"
newepisode="stb-control --device {DEVICE} --touch"
channels="yttv-channels.toml"
```

**yttv-channels.toml**
```toml
[ESPN]
URL="https://tv.youtube.com/watch/prvovIos3DE"

[248]
command="stb-control --device {DEVICE} --link https://tv.youtube.com/watch/3asdo_3a"
recfinished="stb-control --device {DEVICE} --reset"
```

---
### INCLUDES

Any section other than `[VARIABLES]` can reside in a external "sub" configuration file, which can be imported within this section using a TOML array:

```toml
[INCLUDES]
files = [
    "magewell-{SOURCE}.toml",
]
```

---
### TOUCH

Multiple `[TOUCH.name]` blocks can be specified as long as the sub-section `name` suffix is unique. Each `[TOUCH.name]` block can configure the following keys:

* **command** (required) – The system command to run as a background task.
* **delay** (optional) – How long to wait after the recording begins before executing the command for the first time. Allowed formats are `SS`, `M:SS`, `MM:SS`, `H:MM:SS`, and `HH:MM:SS`.
* **frequency** (optional) – How frequently to repeat the command during the lifetime of the recording. Allowed formats are `SS`, `M:SS`, `MM:SS`, `H:MM:SS`, and `HH:MM:SS`.
* **damaged_on_failure** (optional) – If set to `true` and the command returns a non-zero exit code, the recording will be flagged as damaged in MythTV.
* **loglevel** (optional) – Specifies the logging severity level used to record any stdout/stderr output from the command in the application log file.

```toml
[TOUCH.checkinternet]
delay="5:00"
frequency="15:00"
command="/usr/local/bin/stb-control --check-internet"
damaged_on_failure=true
log_level="INFO"
```
---
### SCANNER
You can optionally provide a [SCANNER]/command which will automatically get run whenever you fetch the channels for this external recorder into MythTV.

---
### Loading channels into Myth

To run a scan, navigate to `http://localhost:6544/setupwizard/input-connections` and select your target input. Click the **Scan for Channels** button, and for the **Scan Type**, choose **Import from ExternalRecorder**.

> **NOTE:**
> Options under **Scan Type** will not populate if you haven't clicked the **[Enable Updates]** button near the top of the page. You may also need to click **[Re-open Capture Card]** to force the UI list to update.

When you click **[Start Scan]**, The`[TUNER]/channels file will be parsed and used to populate the channel database in MythTV. If `[SCANNER]/command` is specified, that script or binary will be executed before parsing the [TUNER]/channels file.

----
## Examples

> **IMPORTANT**
> TOML requires that all strings be quoted. It accepts both single and double quotes (`'` and `"`), as well as multi-line quotes (`'''` and `"""`), which work well if the system command itself needs to include quotation marks.

---
### Twitch

**twitch.toml**
```toml
[RECORDER]
command="""
yt-dlp --hls-use-mpegts -o - {URL}
"""

desc="cvlc-{URL}-{CHANNAME}-{CALLSIGN}"

[TUNER]
channels="twitch-channels.toml"
```

**twitch-channels.toml**
```toml
[2]
NAME="TheFutAccountant"
CALLSIGN="Fut2"
URL="https://www.twitch.tv/thefutaccountant"

[3]
NAME="kostahh"
CALLSIGN="Third"
URL="https://www.twitch.tv/kostahh"
```

---
### Magewell2ts
**magewell-1-2-2.toml**
```toml
[VARIABLES]
BOARD=1
INPUT=2
SOURCE=2
DEVICE="roku2"
TUNER="/usr/local/bin/stb-control --device roku2"
CODEC="hevc_qsv --p010"
LOG="-v 4"
REC="/usr/local/bin/magewell2ts -m -b {BOARD} -i {INPUT} -c {CODEC} {LOG}"

[TOUCH.VERIFY]
delay="05:00"
frequency="15:00"
command="/usr/local/bin/roku-control --device {DEVICE} --verify {CALLSIGN}"
damaged_on_failure=true
log_level="INFO"

[INCLUDES]
files = [
    "magewell-{SOURCE}.toml"
]
```
**magewell-2.toml**
```toml
[Recorder]
command_default     = "{REC} -q 22"
command_lowquality  = "{REC} -q 25"
command_highquality = "{REC} -q 16"
desc = "{DEVICE}+[?{BOARD}-?]{INPUT}-{SOURCE}"

[TUNER]
channels = "YTTV-channels.toml"
command = "{TUNER} --link {URL}"
timeout = 60000
```
**YTTV-channels.toml**
```toml
[AMCHD]
URL="https://tv.youtube.com/watch/TMr6pjcOURI"

[AMCPRES]
URL="https://tv.youtube.com/watch/TaIPlh49ke0"

[AMCRSH]
URL="https://tv.youtube.com/watch/poKnRg329og"

[APLHDP]
URL="https://tv.youtube.com/watch/9LjEjSvXVuA"

[BBCAHD]
URL="https://tv.youtube.com/watch/AW1cFldgulo"
```

---
### HDHomeRun
**hdhr-1043FFFF-0.toml**
```toml
[VARIABLES]
HDHR="1043FFFF"
INPUT=1
DEVICE="http://HDHR-{HDHR}.local"

[RECORDER]
command = '''
wget -qO- "{URL}"
'''
desc = "{HDHR}-{INPUT}"

[TUNER]
channels = "/home/mythtv/etc/{HDHR}-channels.toml"

[SCANNER]
command='''
hdhr-externrec-tool --quiet --hdhr HDHR-10A04F10 --channels "/home/mythtv/etc"
'''
```

**1043FFFF-channels.toml**
```toml
[32.1]
NAME="TeleX"
URL="{DEVICE}:5004/auto/v32.1"

[32.2]
NAME="Primo"
URL="{DEVICE}:5004/auto/v32.2"

[32.3]
NAME="Buzzr"
URL="{DEVICE}:5004/auto/v32.3"
```

**WARNING** Streaming from an HDHomeRun tuner this way assumes MythTV has exclusive access to the device. If you plan to share your tuner concurrently with other apps or devices (such as Android TV/Google TV apps), you should instead use Gary Buhrmaster's specialized mythhdhrrecorder script which natively handles multi-client tuner pooling and locking: https://github.com/garybuhrmaster/mythhdhrrecorder.git

If you are okay with giving myth exclusive access to the HDHomeRun tunner, then this tool will auto-create the configuration and channel files for you:
https://github.com/jpoet/hdhr-externrec-tool

---
### Using a shell
**magewell-1-3-2.toml**
```toml
[Variables]
INPUT=3
SOURCE=2
DEVICE="onn1"
TUNER="/usr/local/bin/stb-control --device onn1"
CODEC="hevc_qsv --p010"
LOG="-v 4"

[INCLUDE]
files = [
    "magewell-{SOURCE}-shell.toml",
]
```

**magewell-2-shell.toml**
```toml
[Recorder]
command_default     = "/usr/local/bin/magewell2ts -m -i $INPUT -c $CODEC $LOG -q 21"
command_lowquality  = "/usr/local/bin/magewell2ts -m -i $INPUT -c $CODEC $LOG -q 25"
command_highquality = "/usr/local/bin/magewell2ts -m -i $INPUT -c $CODEC $LOG -q 16"
desc = "{DEVICE}+[?{BOARD}-?]{INPUT}-{SOURCE}[?-{INPUTNAME}?]"
shell = "/usr/bin/bash"

[Tuner]
channels = "YTTV-channels.toml"
command = """
$TUNER --link "$URL"
"""
recfinished="$TUNER --reset"
newepisode="touch /tmp/newepisode"
timeout = 90000

[Touch.internet]
delay="00:30"
frequency="00:35"
command="/usr/local/bin/stb-control --device $DEVICE --check-internet"
damaged_on_failure=true
log_level="DEBUG"
```

**NOTE:**: When using a shell you need to be careful with any variable definitions. When not using a shell, mythexternrecorder does recursive espansion of the variables, but that doesn't happen with a shell.

----
## INI
The previous version of this program used INI style configuration files. Those are still supported but are deprecated. They also lake some of the features that the toml config files support.

----
## Logs
A log file is created in the directory specified by the --logpath. When run via mythbackend, this will automatically be the same path that mythtv uses. The file name will be 'recorder-' followed by [RECORDER]/desc (if it is specified), with the .log extension. If [RECORDER]/desc is not specified, then the name of the config file is used.

Important messages (e.g. NOTICE level or greater) are also passed along to mythbackend and will show up in its log file.

You may want to setup log rotation. For example, in /etc/logrotate.d, create a mythtv file with
```
/var/log/mythtv/recorder*.log {
        daily
        size 100k
        rotate 10
        missingok
        ifempty
        nocreate
#        nocompress
        sharedscripts
        su mythtv mythtv
        createolddir 0755 mythtv mythtv
        olddir /var/log/mythtv/old
        postrotate
                find /var/log/mythtv/old -name "recorder*.log*" -type f -mtime \
+10 -delete
        endscript
}
```

----
## Links

- https://github.com/bennettpeter/MythTV-LeanCapture
- https://github.com/bennettpeter/MythTV-V4L2Capture
- https://github.com/garybuhrmaster/mythhdhrrecorder.git
- https://github.com/jpoet/Magewell2TS
- https://github.com/jpoet/hdhr-externrec-tool
