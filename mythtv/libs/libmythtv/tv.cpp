
#include "tv.h"

#include <QObject>  // for QObject::tr()

/**
 *  \brief Returns a human readable QString representing a TVState.
 *  \param state State to print.
 */
QString StateToString(TVState state)
{
    QString statestr = QString("Unknown(%1)").arg((int)state);
    switch (state) {
        case kState_Error:
            statestr = "Error";
            break;
        case kState_None:
            statestr = "None";
            break;
        case kState_WatchingLiveTV:
            statestr = "WatchingLiveTV";
            break;
        case kState_WatchingPreRecorded:
            statestr = "WatchingPreRecorded";
            break;
        case kState_WatchingVideo:
            statestr = "WatchingVideo";
            break;
        case kState_WatchingDVD:
            statestr = "WatchingDVD";
            break;
        case kState_WatchingBD:
            statestr = "WatchingBD";
            break;
        case kState_WatchingRecording:
            statestr = "WatchingRecording";
            break;
        case kState_RecordingOnly:
            statestr = "RecordingOnly";
            break;
        case kState_ChangingState:
            statestr = "ChangingState";
            break;
    }
    return statestr;
}

QString toTypeString(PictureAdjustType type)
{
    const QString kPicAdjType[] =
    {
        "",
        "",
        QObject::tr("(CH)"),
        QObject::tr("(REC)"),
    };

    return kPicAdjType[(uint)type & 0x3];
}

QString toTitleString(PictureAdjustType type)
{
    const QString kPicAdjTitles[] =
    {
        "",
        QObject::tr("Adjust Playback"),
        QObject::tr("Adjust Recorder"),
        QObject::tr("Adjust Recorder"),
    };

    return kPicAdjTitles[(uint)type & 0x3];
}

QString toString(CommSkipMode type)
{
    const QString kCommSkipTitles[] =
    {
        QObject::tr("Auto-Skip OFF"),
        QObject::tr("Auto-Skip ON"),
        QObject::tr("Auto-Skip Notify"),
    };

    return kCommSkipTitles[(uint)(type) % kCommSkipCount];
}
