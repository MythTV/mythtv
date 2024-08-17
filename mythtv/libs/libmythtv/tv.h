#ifndef TV_H
#define TV_H

#include <cstdint>
#include <QString>

class VBIMode
{
  public:
    enum vbimode_t : std::uint8_t
    {
        None    = 0,
        PAL_TT  = 1,
        NTSC_CC = 2,
    };

    static uint Parse(const QString& vbiformat)
    {
        QString fmt = vbiformat.toLower().left(3);
        if (fmt == "pal")
            return PAL_TT;
        if (fmt == "nts")
            return NTSC_CC;
        return None;
    }
};

/** \brief ChannelChangeDirection is an enumeration of possible channel
 *         changing directions.
 */
enum ChannelChangeDirection : std::uint8_t
{
    CHANNEL_DIRECTION_UP       = 0,
    CHANNEL_DIRECTION_DOWN     = 1,
    CHANNEL_DIRECTION_FAVORITE = 2,
    CHANNEL_DIRECTION_SAME     = 3,
};

/// Used to request ProgramInfo for channel browsing.
enum BrowseDirection : std::int8_t
{
    BROWSE_INVALID  = -1,
    BROWSE_SAME     = 0, ///< Fetch browse information on current channel and time
    BROWSE_UP       = 1, ///< Fetch information on previous channel
    BROWSE_DOWN     = 2, ///< Fetch information on next channel
    BROWSE_LEFT     = 3, ///< Fetch information on current channel in the past
    BROWSE_RIGHT    = 4, ///< Fetch information on current channel in the future
    BROWSE_FAVORITE = 5  ///< Fetch information on the next favorite channel
};

/** \brief TVState is an enumeration of the states used by TV and TVRec.
 */
enum TVState : std::int8_t
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
    kState_WatchingLiveTV = 1,
    /** \brief Watching Pre-recorded is a TV only state for when we are
     *         watching a pre-existing recording.
     */
    kState_WatchingPreRecorded = 2,
    /** \brief Watching Video is the state when we are watching a video and is not
    *             a dvd or BD
    */
    kState_WatchingVideo = 3,
    /** \brief Watching DVD is the state when we are watching a DVD */
    kState_WatchingDVD = 4,
    /** \brief Watching BD is the state when we are watching a BD */
    kState_WatchingBD = 5,
    /** \brief Watching Recording is the state for when we are watching
     *         an in progress recording, but the user does not have control
     *         over the channel and recorder to use.
     */
    kState_WatchingRecording = 6,
    /** \brief Recording Only is a TVRec only state for when we are recording
     *         a program, but there is no one currently watching it.
     */
    kState_RecordingOnly = 7,
    /** \brief This is a placeholder state which we never actually enter,
     *         but is returned by GetState() when we are in the process
     *         of changing the state.
     */
    kState_ChangingState = 8,
};
inline TVState myth_deque_init(const TVState */*state*/) { return (TVState)(0); }

QString StateToString(TVState state);

/** \brief SleepStatus is an enumeration of the awake/sleep status of a slave.
 */
enum SleepStatus : std::uint8_t {
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
};

enum PictureAdjustType : std::uint8_t
{
    kAdjustingPicture_None = 0,
    kAdjustingPicture_Playback,
    kAdjustingPicture_Channel,
    kAdjustingPicture_Recording,
};
QString toTypeString(PictureAdjustType type);
QString toTitleString(PictureAdjustType type);

enum CommSkipMode : std::uint8_t
{
    kCommSkipOff    = 0,
    kCommSkipOn     = 1,
    kCommSkipNotify = 2,
    kCommSkipCount  = 3,
    kCommSkipIncr   = 4,
};
QString toString(CommSkipMode type);
#endif
