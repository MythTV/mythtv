#ifndef TV_H
#define TV_H

#include <qstring.h>

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

const int kLiveTVAutoExpire = 10000;

QString StateToString(TVState state);

typedef enum PictureAdjustType
{
    kAdjustingPicture_None = 0,
    kAdjustingPicture_Playback,
    kAdjustingPicture_Channel,
    kAdjustingPicture_Recording,
} PictureAdjustType;
QString toTypeString(PictureAdjustType type);
QString toTitleString(PictureAdjustType type);

typedef enum PictureAttribute
{
    kPictureAttribute_None = 0,
    kPictureAttribute_MIN = 1,
    kPictureAttribute_Brightness = 1,
    kPictureAttribute_Contrast,
    kPictureAttribute_Colour,
    kPictureAttribute_Hue,
    kPictureAttribute_Volume,
    kPictureAttribute_MAX
} PictureAttribute;
QString toString(PictureAttribute index);

#endif
