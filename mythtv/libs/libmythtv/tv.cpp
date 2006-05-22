#include <algorithm>
using namespace std;

#include "tv.h"
#include "tv_play.h"
#include "tv_rec.h"

/** \fn StateToString(TVState state)
 *  \brief Returns a human readable QString representing a TVState.
 *  \param state State to print.
 */
QString StateToString(TVState state)
{
    QString statestr = QString("Unknown(%1)").arg((int)state);
    switch (state) {
        case kState_Error: statestr = "Error"; break;
        case kState_None: statestr = "None"; break;
        case kState_WatchingLiveTV: statestr = "WatchingLiveTV"; break;
        case kState_WatchingPreRecorded:
            statestr = "WatchingPreRecorded"; break;
        case kState_WatchingRecording: statestr = "WatchingRecording"; break;
        case kState_RecordingOnly: statestr = "RecordingOnly"; break;
        case kState_ChangingState: statestr = "ChangingState"; break;
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

    return kPicAdjType[(int)type & 0x3];
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

    return kPicAdjTitles[(int)type & 0x3];
}

QString toString(PictureAttribute index)
{
    const QString tbl[] =
    {
        QObject::tr("None"),
        QObject::tr("Brightness"),
        QObject::tr("Contrast"),
        QObject::tr("Colour"),
        QObject::tr("Hue"),
        QObject::tr("Volume"),
        QObject::tr("MAX"),
    };

    int i = (int)index;
    i = max(i, (int) kPictureAttribute_None);
    i = min(i, (int) kPictureAttribute_MAX);

    return tbl[i];
}
