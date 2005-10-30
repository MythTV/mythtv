/**
\mainpage MythTV Architecture

\section intro Introduction

Over the last couple years %MythTV has grown into a rather large
application. The purpose of these pages is to provide a portal 
into the code for developers to get their heads around it. 
This is intended for both those new to %MythTV and experienced 
with it to get familiar with different aspects of the code base.

If you are just looking for the code formatting standards, 
see the unofficial mythtv.info page 
<a href="http://www.mythtv.info/moin.cgi/CodingStandards">
coding standards</a>. If you are looking for the 
<a href="http://svn.mythtv.org/trac/wiki/TicketHowTo">bug tracker</a>,
it can be found on the official pages. 
If you haven't already, you should subscribe to
the <a href="http://www.mythtv.org/mailman/listinfo/mythtv-dev/">
developer mailing list</a>

If you just stumbled onto the developer pages
by accident, maybe you want to go to the official
<a href="http://www.mythtv.org/modules.php?name=MythInstall">%MythTV
Installation page</a>. There is also a good unofficial 
<a href="http://wilsonet.com/mythtv/fcmyth.php">Fedora %MythTV installation</a> page,
and a 
<a href="http://dev.gentoo.org/~cardoe/mythtv/">Gentoo %MythTV installation</a> page.

If you are new to Qt programming it is essential that you keep in mind
that almost all Qt objects are not thread-safe, including QString.
Almost all Qt container objects, including QString, make shallow copies
on assignment, the two copies of the object must only be used in one
thread unless you use a lock on the object. You can use the 
<a href="http://doc.trolltech.com/3.1/qdeepcopy.html">QDeepCopy</a>
template on most Qt containers to make a copy you can use in another
thread.

There are also special dangers when 
\ref qobject_dangers "using QObject" outside the Qt event thread.

\section libs Libraries

%MythTV is divided up into eight libraries:
<dl>
  <dt>libmyth                <dd>%MythTV Plugin library.
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

\section db Database Schema
The database schema is documented here \ref db_schema.

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
from the database automagically when used in %MythTV's window classes.
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

/** \defgroup qobject_dangers QObject is dangerous for your health

QObject derived classes can be quite useful, they can send and receive
signals, get keyboard events, translate strings into another language
and use QTimers.

But they also can't be \ref qobject_delete "deleted" easily, they 
are not \ref qobject_threadsafe "thread-safe", can't participate
as equals in multiple \ref qobject_inheritence "inheritence", can
not be used at all in virtual \ref qobject_inheritence "inheritence".

\section qobject_delete Deleting QObjects

The problem with deleting QObjects has to do with signals, and events.
These can arrive from any thread, and often arrive from the Qt event
thread even if you have not explicitly requested them.

If you have not explicitly connected any signals or slots, and the 
only events you might get are dispatched with qApp->postEvent(),
not qApp->sendEvent(), then it is safe to delete the QObject in the
Qt event thread. If your QObject is a GUI object you've passed 
given to Qt for it to delete all is good. If your object is only 
deleted in response to a posted event or a signal from the Qt event
thread (say from a QTimer()) then you are OK as well.

If your class may be deleted outside a Qt thread, but it does not
explicitly connect any signals or slots, and the only events it
might get are dispatched with qApp->postEvent(), then you can 
safely use deleteLater() to queue your object up for deletion
in the event thread.

%MythTV is a very much a multithreaded program so you may need to have
a QObject that may get events from another thread if you send signals
but do not get any signals and or events from outside the Qt event
thread, then you can use deleteLater() or delete depending on whether
you are deleteing the QObject in the event thread, see above. But
if you are not in the Qt event thread then you need to call disconnect()
in the thread that is sending the signals, before calling deleteLater().
This prevents your object from sending events after you consider it 
deleted (and begin to delete other QObjects that may still be getting
signals from your object.)

What about if you are getting events via qApp->sendEvent() or a signal
from another thread? In this case things get complicated, so we highly
discourage this in %MythTV and prefer that you use callbacks if at all
possible. But in say you need to do this, then you need to disconnect
all the signals being sent to your QObject. But disconnect() itself
is not atomic. That is you may still get a signal after you disconnect
it. What you need to is disconnect the signal from your slot in the 
thread that is sending the signal. This prevents the signal from
being emitted while you are disconnecting. Doing this gracefully is
left as an excersize for the reader.

Ok, so the above is not entirely complete, for instance you could wrap
every signal emit in an instance lock and reimplement a disconnect
that uses that instance lock. But if you've figured this or another
solution out already all you need to know is that almost all Qt
objects are reenterant but not thread-safe, and that QObjects recieve
events and signals from the Qt event thread and from other threads
if you use qApp->sendEvent() or send signals to it from another thread.

\section qobject_threadsafe QObject thread-safety

The only thread-safe classes in Qt are QThread and the mutex classes.
when you call any non-static QObject method you must have a lock on
it or make sure only thread ever calls that function.

QObject itself is pretty safe because when you assign one QObject to
another you make a copy. Qt's container classes, including QString
do not make a copy but rather pass a reference counted pointer to
the data when you assign one to another. This reference counter is
not protected by a mutex so you can not use regular assignment when
passing a QString or other Qt container from one thread to another.

In order to use these classes safely you must use 
<a href="http://doc.trolltech.com/3.1/qdeepcopy.html">QDeepCopy</a>
when passing them from one thread to another. This is thankfully quite
easy to use.

\code
  QString original = "hello world";
  QString unsafe = original;
  QString safe = QDeepCopy<QString>(original);
\endcode

In this case safe and original can be used by two seperate
threads, while unsafe can only be used in the same thread 
as originalStr.

The QDeepCopy template will work on the most popular Qt containers,
for the ones it doesn't support you will have to copy manually.

\section qobject_inheritence QObject inheritence

You can not inherit more than one QObject derived class.
This is because of how signals and slots are implemented. What
happens is that each slot is transformed into an integer that
is used to find the right function pointer quickly when a signal
is sent. But if you have more than one QObject in the inheritence
tree the integers will alias so that signals intended for the
second slot in parent class B might get sent to the second slot
in parent class A instead. Badness ensues.

For the similar reason you can not inherit a QObject derived
class virtually, in this case the badness is a little more
complicated. The result is the same however, signals arrive
at the wrong slot. Usually, what happens is that the developer
tries to inherit two QObject derived classes but gets linking
errors, so they make one or more of the classes virtual, or
inherit a QObject non-virtually but then also inherit one
or more QObject derived classes virtually. The linker doesn't
complain, but strange unexplainable behaviour ensues when
the application is run and signals start showing up on the
wrong slots.

*/
