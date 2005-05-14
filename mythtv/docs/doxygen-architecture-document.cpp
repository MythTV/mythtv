/**
\mainpage MythTV Architecture

\section intro Introduction

Over the last couple years %MythTV has grown into a rather large
application. The purpose of these pages is to provide a portal 
into the code for developers to get their heads around it. 
This is intended for both those new to %MythTV and experienced 
with it to get familiar with different aspects of the code base.

Before we get started, if you are just looking for the code 
formatting standards, see the unofficial mythtv.info page 
<a href="http://www.mythtv.info/moin.cgi/CodingStandards">
Coding Standards</a>. If you are looking for the 
<a href="http://www.mythtv.org/bugs/">Bug tracker</a>, it
can be found on the official pages. And you should subscribe to
the <a href="http://www.mythtv.org/mailman/listinfo/mythtv-dev/">
developer mailing list</a>, if you haven't already.
And, of course, if you just stumbled onto the developer pages
by accident, maybe you want to go to the 
<a href="http://www.mythtv.org/modules.php?name=MythInstall">%MythTV
Installation page</a>.

\section libs Libraries

%MythTV is divided up into eight libraries:
<dl>
  <dt>libmyth                <dd>Core Myth library.
      The \ref database_subsystem "database",
      \ref audio_subsystem "audio",
      \ref lcd_subsystem "LCD",
      \ref osd_subsystem "OSD",
      \ref lirc_subsystem "lirc", and the
      \ref myth_network_protocol "myth network protocol" are supported by libmyth.
  <dt>libmythtv              <dd>Core %MythTV library.
      The 
      \ref recorder_subsystem "recorders" and 
      \ref av_player_subsystem "A/V players" are supported by libmythtv.
  <dt>libavcodec/libavformat <dd>This is the ffmpeg A/V decoding library (aka avlib).
      <a href="http://ffmpeg.sourceforge.net/documentation.php">Documented Externally</a>.
  <dt>libmythmpeg2           <dd>Alternate MPEG-1/2 A/V decoding library.
      <a href="http://libmpeg2.sourceforge.net/">External Website</a>.
  <dt>libmythsamplerate      <dd>Audio resampling library
      <a href="http://www.mega-nerd.com/SRC/api.html">Documented Externally</a>.
      We use this to support a different output sample rates than the sample
      rate used in the audio streams we play.
  <dt>libmythsoundtouch</a>  <dd>Pitch preserving audio resampling library.
      <a href="http://sky.prohosting.com/oparviai/soundtouch/">External Website</a>.
      We use this for the time-stretch feature.
  <dt>libmythui              <dd>Next Gen UI rendering library, not yet being used.
</dl>
Two libraries libmythmpeg2 and libmythsamplerate appear redundant, but
libmpeg2 decodes MPEG-2 more quickly than ffmpeg on some systems, and
libmythsamplerate resamples audio with better quality when we only need
to match the hardware sample rate to the A/V streams audio sample rate.

\section apps Applications
%MythTV contains 12 applications:

<dl>
  <dt>mythbackend      <dd>This is the backend which runs the recorders.
  <dt>mythfrontend     <dd>This is the frontend which is the main application for viewing
                           programs and using the %MythTV plugins.
  <dt>mythtv           <dd>This is the "External Player" used to play videos from
                           within mythfrontend that are not proper programs,
                           such as your home movies.
  <dt>mythtvosd        <dd>This is used externally by programs that want to pop-up
                            an <i>on screen display</i> in %MythTV while one is watching a
                           recording.
  <dt>mythfilldatabase <dd>This is used both internally and externally to
                           fetch program listings.
                           <a href="http://tms.tribune.com/">Tribune Media</a> provides
                           listings in exchange for demographic information in
                           the USA, and <a href="http://d1.com.au/">D1</a> has
                           donated free listings in Australia as thanks for the
                           %MythTV code used in their <i>Home Media Centre</i>
                           product. Other markets are served by the
                           <a href="http://membled.com/work/apps/xmltv/xmltv">XMLTV</a>
                           web spiders.
  <dt>mythtranscode    <dd>This is used both internally and externally to
                           transcode videos from one format to another. 
                           This is used to shrink HDTV programs to lower
                           quality recordings that match the hardware the
                           user has.
  <dt>mythjobqueue     <dd>This is used internally by mythfrontend to schedule
                           jobs such as commercial flagging and transcoding.
  <dt>mythcommflag     <dd>This is used internally by mythfrontend to flag
                           commercials.
  <dt>mythepg          <dd>This is used internally by mythfrontend to find
                           upcoming programs to record based on the channel
                           and time.
  <dt>mythprogfind     <dd>This is used internally by mythfrontend to find 
                           programs to record based on the first letter of
                           the program name.
  <dt>mythuitest       <dd>This is a test program for libmythui development.
  <dt>mythlcd          <dd>This is a test program for %MythTV %LCD support.
</dl>

\section fe_plugins Frontend Plugins
<dl>
  <dt>mythbrowser <dd>Provides a simple web browser.
  <dt>mythdvd     <dd>Launches DVD players such as Xine and MPlayer.
  <dt>mythgame    <dd>Launches the xmame classic game system emulator.
  <dt>mythnews    <dd>Browses RSS news feeds.
  <dt>mythweather <dd>Presents your local weather report.
  <dt>mythgallery <dd>A simple picture viewer for your %TV.
  <dt>mythmusic   <dd>A simple music player for your %TV.
  <dt>mythphone   <dd>SIP based video phone.
  <dt>mythvideo   <dd>Video Browser for non-%MythTV recordings.
</dl>

\section be_plugins Backend Plugins
<dl>
  <dt>mythweb     <dd>Provides a PHP based web pages to control mythbackend.
</dl>

 */

/** \defgroup database_subsystem    Database Subsystem
    \todo No one is working on documenting the database subsystem.

There are a few classes that deal directly with the database: 
MythContext, MSqlDatabase, DBStorage, MDBManager, SimpleDBStorage.

And one function UpgradeTVDatabaseSchema() located in dbcheck.cpp.

MythTV's Configurable widgets also do save and restore their values
from the database automagically when used in MythTV's window classes.
 */

/** \defgroup audio_subsystem       Audio Subsystem
    \todo Ed W will be documenting the audio subsystem.
 */

/** \defgroup lcd_subsystem         LCD Subsystem
    \todo No one is working on documenting the LCD Subsystem
 */

/** \defgroup osd_subsystem         OSD Subsystem
    \todo No one is working on documenting the OSD Subsystem
 */

/** \defgroup lirc_subsystem        LIRC Subsystem
    \todo No one is working on documenting the LIRC Subsystem
 */

/** \defgroup myth_network_protocol Myth Network Protocol
    \todo No one is working on documenting the myth network protocol
 */

/** \defgroup recorder_subsystem    Recorder Subsystem
TVRec is the main class for handling recording. 
\todo +++It is passed a ProgramInfo for the current and next recordings IS THIS TRUE? +++,
and in turn creates three main worker classes:
<dl>
  <dt>RecorderBase <dd>Recordings from device into RingBuffer.
  <dt>ChannelBase  <dd>Changes the channel and other recording device attributes. Optional.
  <dt>RingBuffer   <dd>Handles A/V file I/O, including streaming.
</dl>

%TVRec also presents the public face of a recordings to the 
Myth Network Protocol, and hence the rest of %MythTV. This
means that any call to the %RecorderBase, %RingBuffer, or
%ChannelBase is marshalled via methods in the %TVRec.

RecorderBase contains classes for recording %MythTV's 
specialized Nuppel Video files, as well as classes for 
handling various hardware encoding devices such as MPEG2,
HDTV, DVB and Firewire recorders.
ChannelBase meanwhile only contains three subclasses, %Channel
for handling v4l and v4l2 devices, and %DVBChannel and
%FirewireChannel, for handling DVB and Firewire respectively.
Other channel changing hardware use ChannelBase's external
channel changing program support.
Finally, RingBuffer does all reading and writing of A/V files
for both TV (playback) and %TVRec, including streaming
over network connections to the frontend's %RingBuffer.

%TVRec has four active states, the first three of which
correspond to the same states in %TV: kState_WatchingLiveTV,
kState_WatchingPreRecorded, kState_WatchingRecording, and
kState_RecordingOnly.
When watching "Live TV" the recorder records whatever the 
frontend requests and streams it out using the %RingBuffer,
this may be to disk or over the network.
When watching pre-recorded programs %TVRec simply streams a
file on disk out using the %RingBuffer. 
When just watching a recording, %TVRec continues a recording
started as recording-only while simultaneously streaming out
using the %RingBuffer.
Finally, when in the recording-only mode the recording
is only saved to disk using %RingBuffer and no streaming
to the frontend is performed.

%TVRec also has three additional states: kState_Error,
kState_None, kState_ChangingState. The error state allows
%MythTV to know when something has gone wrong. The null or
none state means the recorder is not doing anything and
is ready for commands. Finally, the "ChangingState" state
tells us that a state change request is pending, so other
state changing commands should not be issued.

 */

/** \defgroup av_player_subsystem   A/V Player Subsystem

TV is the main class for handling playback.
It instanciates several important classes:
<dl>
<dt>NuppelVideoPlayer <dd>Decodes audio and video from the RingBuffer, then plays and displays them, resp.
<dt>RemoteEncoder     <dd>Communicates with TVRec on the backend.
<dt>OSD               <dd>Creates on-screen-display for NuppelVideoPlayer to display.
<dt>RingBuffer        <dd>Handles A/V file I/O, including streaming.
</dl>

NuppelVideoPlayer is a bit of a misnomer. This class plays all video formats
that %MythTV can handle. It creates AudioOutput to handle the audio, a FilterChain
to perform post-processing on the video, a DecoderBase class to do the actual
video decoding, and a VideoOutput class to do the actual video output. See
that class for more on what it does.

%TV has three active states that correspond to the same states in TVRec:
kState_WatchingLiveTV, kState_WatchingPreRecorded, kState_WatchingRecording.
When watching "LiveTV" the %TVRec via RemoteEncoder responds to channel
changing and other commands from %TV while streaming the recording to 
%TV's %RingBuffer.
When watching a pre-recorded stream, a recording is streamed from TVRec's
%RingBuffer to %TV's %RingBuffer, but no channel changing commands can be
sent to TVRec.
When watching an 'in-progress' recording things work pretty much as when
watching a pre-recorded stream, except %TV must be prevented from seeking
too far ahead in the program, and keyframe and commercial flags must be 
synced from the recorder periodically.

%TV also has three additional states: kState_Error,
kState_None, kState_ChangingState. The error state allows
%MythTV to know when something has gone wrong. The null or
none state means the player is not doing anything and
is ready for commands. Finally, the "ChangingState" state
tells us that a state change request is pending, so other
state changing commands should not be issued.

 */



