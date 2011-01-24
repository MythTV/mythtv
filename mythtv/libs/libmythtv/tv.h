#ifndef TV_H
#define TV_H

#include <QString>

#include "videoouttypes.h"

/////////////// These are globally registered in MythMainWindow ////////////////
#define ACTION_0 "0"
#define ACTION_1 "1"
#define ACTION_2 "2"
#define ACTION_3 "3"
#define ACTION_4 "4"
#define ACTION_5 "5"
#define ACTION_6 "6"
#define ACTION_7 "7"
#define ACTION_8 "8"
#define ACTION_9 "9"

#define ACTION_SELECT "SELECT"
#define ACTION_UP     "UP"
#define ACTION_DOWN   "DOWN"
#define ACTION_LEFT   "LEFT"
#define ACTION_RIGHT  "RIGHT"
////////////////////////////////////////////////////////////////////////////////

#define ACTION_CHANNELUP   "CHANNELUP"
#define ACTION_CHANNELDOWN "CHANNELDOWN"
#define ACTION_SEEKRWND    "SEEKRWND"
#define ACTION_SEEKFFWD    "SEEKFFWD"

#define ACTION_TOGGLESTUDIOLEVELS "TOGGLESTUDIOLEVELS"
#define ACTION_TOGGLEUPMIX        "TOGGLEUPMIX"
#define ACTION_TOGGLERECORD       "TOGGLERECORD"
#define ACTION_TOGGLEFAV          "TOGGLEFAV"
#define ACTION_TOGGLECHANCONTROLS "TOGGLECHANCONTROLS"
#define ACTION_TOGGLERECCONTROLS  "TOGGLERECCONTROLS"
#define ACTION_TOGGLEINPUTS       "TOGGLEINPUTS"

#define ACTION_GUIDE             "GUIDE"
#define ACTION_FINDER            "FINDER"
#define ACTION_TOGGLESLEEP       QString("TOGGLESLEEP")
#define ACTION_PLAY              "PLAY"
#define ACTION_JUMPPREV          "JUMPPREV"
#define ACTION_JUMPREC           QString("JUMPREC")
#define ACTION_VIEWSCHEDULED     "VIEWSCHEDULED"
#define ACTION_SIGNALMON         "SIGNALMON"

#define ACTION_JUMPTODVDROOTMENU    "JUMPTODVDROOTMENU"
#define ACTION_JUMPTOPOPUPMENU      "JUMPTOPOPUPMENU"
#define ACTION_JUMPTODVDCHAPTERMENU "JUMPTODVDCHAPTERMENU"
#define ACTION_JUMPTODVDTITLEMENU   "JUMPTODVDTITLEMENU"

#define ACTION_SCREENSHOT        "SCREENSHOT"
#define ACTION_EXITSHOWNOPROMPTS "EXITSHOWNOPROMPTS"

/* Interactive Television keys */
#define ACTION_MENURED    "MENURED"
#define ACTION_MENUGREEN  "MENUGREEN"
#define ACTION_MENUYELLOW "MENYELLOW"
#define ACTION_MENUBLUE   "MENUBLUE"
#define ACTION_TEXTEXIT   "TEXTEXIT"
#define ACTION_MENUTEXT   "MENUTEXT"
#define ACTION_MENUEPG    "MENUEPG"

/* Editing keys */
#define ACTION_CLEARMAP     "CLEARMAP"
#define ACTION_INVERTMAP    "INVERTMAP"
#define ACTION_SAVEMAP      "SAVEMAP"
#define ACTION_LOADCOMMSKIP "LOADCOMMSKIP"
#define ACTION_NEXTCUT      "NEXTCUT"
#define ACTION_PREVCUT      "PREVCUT"
#define ACTION_BIGJUMPREW   "BIGJUMPREW"
#define ACTION_BIGJUMPFWD   "BIGJUMPFWD"

/* Teletext keys */
#define ACTION_NEXTPAGE         "NEXTPAGE"
#define ACTION_PREVPAGE         "PREVPAGE"
#define ACTION_NEXTSUBPAGE      "NEXTSUBPAGE"
#define ACTION_PREVSUBPAGE      "PREVSUBPAGE"
#define ACTION_TOGGLETT         "TOGGLETT"
#define ACTION_MENUWHITE        "MENUWHITE"
#define ACTION_TOGGLEBACKGROUND "TOGGLEBACKGROUND"
#define ACTION_REVEAL           "REVEAL"

/** \brief ChannelChangeDirection is an enumeration of possible channel
 *         changing directions.
 */
typedef enum
{
    CHANNEL_DIRECTION_UP       = 0,
    CHANNEL_DIRECTION_DOWN     = 1,
    CHANNEL_DIRECTION_FAVORITE = 2,
    CHANNEL_DIRECTION_SAME     = 3,
} ChannelChangeDirection;

/// Used to request ProgramInfo for channel browsing.
typedef enum BrowseDirections
{
    BROWSE_INVALID = -1,
    BROWSE_SAME = 0, ///< Fetch browse information on current channel and time
    BROWSE_UP,       ///< Fetch information on previous channel
    BROWSE_DOWN,     ///< Fetch information on next channel
    BROWSE_LEFT,     ///< Fetch information on current channel in the past
    BROWSE_RIGHT,    ///< Fetch information on current channel in the future
    BROWSE_FAVORITE  ///< Fetch information on the next favorite channel
} BrowseDirection;

/** \brief TVState is an enumeration of the states used by TV and TVRec.
 */
typedef enum
{
    /** \brief Error State, if we ever try to enter this state errored is set.
     */
    kState_Error = -1,
    /** \brief None State, this is the initial state in both TV and TVRec, it
     *         indicates that we are ready to change to some other state.
     */
    kState_None = 0,
    /** \brief Watching LiveTV is the state for when we are watching a
     *         recording and the user has control over the channel and
     *         the recorder to use.
     */
    kState_WatchingLiveTV,
    /** \brief Watching Pre-recorded is a TV only state for when we are
     *         watching a pre-existing recording.
     */
    kState_WatchingPreRecorded,
    /** \brief Watching Video is the state when we are watching a video and is not
    *             a dvd or BD
    */
    kState_WatchingVideo,
    /** \brief Watching DVD is the state when we are watching a DVD */
    kState_WatchingDVD,
    /** \brief Watching BD is the state when we are watching a BD */
    kState_WatchingBD,
    /** \brief Watching Recording is the state for when we are watching
     *         an in progress recording, but the user does not have control
     *         over the channel and recorder to use.
     */
    kState_WatchingRecording,
    /** \brief Recording Only is a TVRec only state for when we are recording
     *         a program, but there is no one currently watching it.
     */
    kState_RecordingOnly,
    /** \brief This is a placeholder state which we never actualy enter,
     *         but is returned by GetState() when we are in the process
     *         of changing the state.
     */
    kState_ChangingState,
} TVState;

QString StateToString(TVState state);

/** \brief SleepStatus is an enumeration of the awake/sleep status of a slave.
 */
typedef enum SleepStatus {
	/** \brief A slave is awake when it is connected to the master
      */
    sStatus_Awake         = 0x0,
	/** \brief A slave is considered asleep when it is not awake and not
      *        undefined.
      */
    sStatus_Asleep        = 0x1,
	/** \brief A slave is marked as falling asleep when told to shutdown by
      *        the master.
      */
    sStatus_FallingAsleep = 0x3,
	/** \brief A slave is marked as waking when the master runs the slave's
      *        wakeup command.
      */
    sStatus_Waking        = 0x5,
	/** \brief A slave's sleep status is undefined when it has never connected
      *        to the master backend or is not able to be put to sleep and
      *        awakened.
      */
    sStatus_Undefined     = 0x8
} SleepStatus;

typedef enum PictureAdjustType
{
    kAdjustingPicture_None = 0,
    kAdjustingPicture_Playback,
    kAdjustingPicture_Channel,
    kAdjustingPicture_Recording,
} PictureAdjustType;
QString toTypeString(PictureAdjustType type);
QString toTitleString(PictureAdjustType type);

typedef enum
{
    kCommSkipOff    = 0,
    kCommSkipOn     = 1,
    kCommSkipNotify = 2,
    kCommSkipCount,
    kCommSkipIncr,
} CommSkipMode;
QString toString(CommSkipMode type);
#endif
